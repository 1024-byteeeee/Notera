#include "features/library/LibraryService.h"

#include <algorithm>
#include <QFileInfo>
#include <QPdfDocument>
#include <QUrl>
#include <QUuid>

#include "services/FileService.h"

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
        const auto url = value.toUrl();
        if (!url.isLocalFile()) {
            emit errorOccurred(QStringLiteral("只能导入本地乐谱文件。"));
            continue;
        }
        importFile(url.toLocalFile());
    }
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

void LibraryService::importFile(const QString& sourcePath)
{
    if (!FileService::isSupportedScoreFile(sourcePath)) {
        emit errorOccurred(QStringLiteral("Notera 支持 PDF、JPG、JPEG 和 PNG 格式的乐谱。"));
        return;
    }

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
        .title = source.completeBaseName(),
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
    m_thumbnailGenerator.generate(score.id, score.filePath, score.fileType);
    reload();
    emit noticeOccurred(QStringLiteral("已导入 %1").arg(score.title));
}
