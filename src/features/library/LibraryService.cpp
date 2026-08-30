#include "features/library/LibraryService.h"

#include <algorithm>
#include <QFile>
#include <QFileInfo>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPdfDocument>
#include <QTemporaryFile>
#include <QUrl>
#include <QUuid>

#include "services/FileService.h"
#include "platform/AppDataPaths.h"

namespace {

int pageCountForFile(const QString& filePath, const QString& fileType)
{
    if (fileType != QStringLiteral("pdf")) {
        return 1;
    }

    QPdfDocument document;
    document.load(filePath);
    return document.status() == QPdfDocument::Status::Ready ? std::max(1, document.pageCount()) : 1;
}

} // namespace

LibraryService::LibraryService(QObject* parent)
    : QObject(parent)
    , m_repository(m_databaseService.database())
    , m_scores(this)
    , m_thumbnailGenerator(this)
{
    QString error;
    if (!m_databaseService.initialize(&error)) {
        emit errorOccurred(QStringLiteral("初始化乐谱库数据库失败。"));
        return;
    }
    m_repository = ScoreRepository(m_databaseService.database());
    connect(&m_thumbnailGenerator, &ThumbnailGenerator::generated, this, [this](const QString& scoreId, const QString& path) {
        QString error;
        if (!m_repository.updateThumbnail(scoreId, path, &error)) {
            emit errorOccurred(QStringLiteral("更新乐谱缩略图失败。"));
            return;
        }
        reload();
    });
    connect(&m_thumbnailGenerator, &ThumbnailGenerator::failed, this, [this](const QString&, const QString& message) {
        emit errorOccurred(message);
    });
    reload();
}

ScoreListModel* LibraryService::scores()
{
    return &m_scores;
}

QString LibraryService::searchQuery() const
{
    return m_searchQuery;
}

void LibraryService::setSearchQuery(const QString& searchQuery)
{
    if (m_searchQuery == searchQuery) return;
    m_searchQuery = searchQuery;
    emit searchQueryChanged();
    reload();
}

void LibraryService::importUrls(const QVariantList& urls)
{
    for (const auto& value : urls) {
        QUrl url = value.toUrl();
        if (!url.isValid() || url.scheme().isEmpty()) {
            const auto input = value.toString().trimmed();
            url = QFileInfo(input).exists() ? QUrl::fromLocalFile(input) : QUrl::fromUserInput(input);
        }
        if (url.isLocalFile()) {
            importFile(url.toLocalFile());
        } else if (url.scheme() == QStringLiteral("http") || url.scheme() == QStringLiteral("https")) {
            downloadFile(url);
        } else {
            emit errorOccurred(QStringLiteral("无法识别导入地址：%1").arg(value.toString()));
        }
    }
}

void LibraryService::importUrl(const QString& url)
{
    importUrls({url});
}

void LibraryService::downloadFile(const QUrl& url)
{
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    auto* const reply = m_network.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, url] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit errorOccurred(QStringLiteral("下载失败：%1").arg(reply->errorString()));
            return;
        }
        constexpr qint64 MaximumDownloadSize = 512LL * 1024LL * 1024LL;
        const auto declaredSize = reply->header(QNetworkRequest::ContentLengthHeader).toLongLong();
        if (declaredSize > MaximumDownloadSize) {
            emit errorOccurred(QStringLiteral("下载文件超过 512 MB。"));
            return;
        }
        const auto data = reply->readAll();
        if (data.isEmpty() || data.size() > MaximumDownloadSize) {
            emit errorOccurred(QStringLiteral("下载文件为空或超过 512 MB。"));
            return;
        }
        const auto suffix = QFileInfo(url.path()).suffix().toLower();
        QTemporaryFile temporary(AppDataPaths::cacheDirectory() + QStringLiteral("/download-XXXXXX")
            + (suffix.isEmpty() ? QString() : QLatin1Char('.') + suffix));
        if (!temporary.open() || temporary.write(data) != data.size()) {
            emit errorOccurred(QStringLiteral("无法保存下载的临时文件。"));
            return;
        }
        const auto temporaryPath = temporary.fileName();
        temporary.close();
        importFile(temporaryPath, QFileInfo(url.path()).completeBaseName());
    });
}

void LibraryService::toggleFavorite(const QString& scoreId, const bool favorite)
{
    QString error;
    if (!m_repository.setFavorite(scoreId, favorite, &error)) {
        emit errorOccurred(QStringLiteral("更新收藏状态失败。"));
        return;
    }
    reload();
}

void LibraryService::renameScore(const QString& scoreId, const QString& title)
{
    if (title.trimmed().isEmpty()) {
        emit errorOccurred(QStringLiteral("乐谱名称不能为空。"));
        return;
    }
    QString error;
    if (!m_repository.rename(scoreId, title, &error)) {
        emit errorOccurred(QStringLiteral("重命名乐谱失败。"));
        return;
    }
    reload();
}

void LibraryService::deleteScore(const QString& scoreId, const QString& filePath, const QString& thumbnailPath)
{
    QString error;
    if (!FileService::removeFile(filePath, &error) || !FileService::removeFile(thumbnailPath, &error)) {
        emit errorOccurred(error);
        return;
    }
    if (!m_repository.remove(scoreId, &error)) {
        emit errorOccurred(QStringLiteral("删除乐谱记录失败。"));
        return;
    }
    reload();
}

void LibraryService::reload()
{
    QString error;
    const auto scores = m_repository.list(m_searchQuery, &error);
    if (!error.isEmpty()) {
        emit errorOccurred(QStringLiteral("加载乐谱库失败。"));
        return;
    }
    m_scores.replaceAll(scores);
}

void LibraryService::importFile(const QString& sourcePath, const QString& titleOverride)
{
    const QFileInfo source(sourcePath);
    const auto scoreId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QString error;
    const auto storedPath = FileService::copyScoreIntoLibrary(sourcePath, scoreId, &error);
    if (storedPath.isEmpty()) {
        emit errorOccurred(error);
        return;
    }

    const auto now = QDateTime::currentDateTimeUtc();
    Score score {
        .id = scoreId,
        .title = titleOverride.trimmed().isEmpty() ? source.completeBaseName() : titleOverride.trimmed(),
        .fileName = QFileInfo(storedPath).fileName(),
        .filePath = storedPath,
        .fileType = FileService::canonicalSuffix(sourcePath),
        .pageCount = pageCountForFile(storedPath, FileService::canonicalSuffix(sourcePath)),
        .createdAt = now,
        .updatedAt = now
    };
    if (!m_repository.insert(score, &error)) {
        FileService::removeFile(storedPath, &error);
        emit errorOccurred(QStringLiteral("将乐谱添加到乐谱库失败。"));
        return;
    }
    if (FileService::isSupportedScoreFile(score.filePath)) {
        m_thumbnailGenerator.generate(score.id, score.filePath, score.fileType);
    }
    reload();
    emit noticeOccurred(QStringLiteral("已导入 %1").arg(score.title));
}
