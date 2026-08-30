#include "features/library/LibraryService.h"

#include <algorithm>
#include <QBuffer>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QPainter>
#include <QPdfDocument>
#include <QTemporaryFile>
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
    , m_folders(this)
    , m_tags(this)
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
    reloadFolders();
    reloadTags();
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

QString LibraryService::filterMode() const
{
    return m_filterMode;
}

void LibraryService::setFilterMode(const QString& mode)
{
    if (m_filterMode == mode) return;
    m_filterMode = mode;
    emit filterModeChanged();
    reload();
}

NamedListModel* LibraryService::folders()
{
    return &m_folders;
}

NamedListModel* LibraryService::tags()
{
    return &m_tags;
}

void LibraryService::createFolder(const QString& name)
{
    if (name.trimmed().isEmpty()) {
        emit errorOccurred(QStringLiteral("文件夹名称不能为空。"));
        return;
    }
    QString error;
    if (!m_repository.createFolder(name, &error)) {
        emit errorOccurred(QStringLiteral("创建文件夹失败。"));
        return;
    }
    reloadFolders();
    emit noticeOccurred(QStringLiteral("已创建文件夹 %1").arg(name.trimmed()));
}

void LibraryService::createTag(const QString& name)
{
    if (name.trimmed().isEmpty()) {
        emit errorOccurred(QStringLiteral("标签名称不能为空。"));
        return;
    }
    QString error;
    if (!m_repository.createTag(name, &error)) {
        emit errorOccurred(QStringLiteral("创建标签失败。"));
        return;
    }
    reloadTags();
    emit noticeOccurred(QStringLiteral("已创建标签 %1").arg(name.trimmed()));
}

void LibraryService::renameFolder(const QString& folderId, const QString& name)
{
    if (name.trimmed().isEmpty()) {
        emit errorOccurred(QStringLiteral("文件夹名称不能为空。"));
        return;
    }
    QString error;
    if (!m_repository.renameFolder(folderId, name, &error)) {
        emit errorOccurred(QStringLiteral("重命名文件夹失败。"));
        return;
    }
    reloadFolders();
    emit noticeOccurred(QStringLiteral("已重命名为 %1").arg(name.trimmed()));
}

void LibraryService::deleteFolder(const QString& folderId)
{
    QString error;
    if (!m_repository.deleteFolder(folderId, &error)) {
        emit errorOccurred(QStringLiteral("删除文件夹失败。"));
        return;
    }
    if (m_filterMode.startsWith(QStringLiteral("folder:"))) {
        m_filterMode = QStringLiteral("all");
        emit filterModeChanged();
    }
    reloadFolders();
    reload();
    emit noticeOccurred(QStringLiteral("文件夹已删除"));
}

void LibraryService::renameTag(const QString& tagId, const QString& name)
{
    if (name.trimmed().isEmpty()) {
        emit errorOccurred(QStringLiteral("标签名称不能为空。"));
        return;
    }
    QString error;
    if (!m_repository.renameTag(tagId, name, &error)) {
        emit errorOccurred(QStringLiteral("重命名标签失败。"));
        return;
    }
    reloadTags();
    emit noticeOccurred(QStringLiteral("已重命名为 %1").arg(name.trimmed()));
}

void LibraryService::deleteTag(const QString& tagId)
{
    QString error;
    if (!m_repository.deleteTag(tagId, &error)) {
        emit errorOccurred(QStringLiteral("删除标签失败。"));
        return;
    }
    if (m_filterMode.startsWith(QStringLiteral("tag:"))) {
        m_filterMode = QStringLiteral("all");
        emit filterModeChanged();
    }
    reloadTags();
    reload();
    emit noticeOccurred(QStringLiteral("标签已删除"));
}

void LibraryService::requestImport()
{
    emit importRequested();
}

void LibraryService::importLocalFile(const QUrl& url)
{
    if (!url.isValid()) {
        emit errorOccurred(QStringLiteral("请选择电脑上的文件。"));
        return;
    }

    QString localPath;
    if (url.isLocalFile()) {
        localPath = url.toLocalFile();
    } else if (url.scheme().isEmpty()) {
        // QML FileDialog / DropArea 在部分平台下可能传入纯路径而非 file:// URL
        localPath = url.path();
        if (localPath.isEmpty()) {
            localPath = url.toString();
        }
        if (!QFileInfo::exists(localPath)) {
            emit errorOccurred(QStringLiteral("请选择电脑上的文件。"));
            return;
        }
    } else {
        emit errorOccurred(QStringLiteral("请选择电脑上的文件。"));
        return;
    }

    importFile(localPath);
}

void LibraryService::importAndStitchImages(const QVariantList& urls)
{
    if (urls.size() < 2) {
        emit errorOccurred(QStringLiteral("拼接导入需要至少选择两张图片。"));
        return;
    }

    // 解析并加载所有图片
    QList<QImage> images;
    for (const auto& variant : urls) {
        const QUrl url = variant.toUrl();
        QString localPath;
        if (url.isLocalFile()) {
            localPath = url.toLocalFile();
        } else if (url.scheme().isEmpty()) {
            localPath = url.path();
            if (localPath.isEmpty()) localPath = url.toString();
        }
        if (localPath.isEmpty() || !QFileInfo::exists(localPath)) {
            emit errorOccurred(QStringLiteral("无法读取图片文件。"));
            return;
        }
        QImage img(localPath);
        if (img.isNull()) {
            emit errorOccurred(QStringLiteral("无法加载图片：%1").arg(QFileInfo(localPath).fileName()));
            return;
        }
        images.append(img);
    }

    // 计算拼接尺寸：宽度取最大值，高度累加
    int maxWidth = 0;
    qint64 totalHeight = 0;
    for (const auto& img : images) {
        maxWidth = qMax(maxWidth, img.width());
        totalHeight += img.height();
    }

    if (maxWidth <= 0 || totalHeight <= 0) {
        emit errorOccurred(QStringLiteral("图片尺寸无效。"));
        return;
    }

    // 安全上限：防止超大拼接图导致内存问题
    if (totalHeight > 65536 || maxWidth > 16384) {
        emit errorOccurred(QStringLiteral("拼接后图片尺寸过大，请减少图片数量。"));
        return;
    }

    // 创建拼接画布（白色背景）
    QImage stitched(maxWidth, static_cast<int>(totalHeight), QImage::Format_ARGB32);
    stitched.fill(Qt::white);

    QPainter painter(&stitched);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    int y = 0;
    for (const auto& img : images) {
        // 水平居中绘制
        int x = (maxWidth - img.width()) / 2;
        painter.drawImage(x, y, img);
        y += img.height();
    }
    painter.end();

    // 保存到临时文件
    QTemporaryFile tempFile(QDir::tempPath() + "/notera_stitch_XXXXXX.png");
    if (!tempFile.open()) {
        emit errorOccurred(QStringLiteral("创建临时文件失败。"));
        return;
    }
    if (!stitched.save(tempFile.fileName(), "PNG")) {
        emit errorOccurred(QStringLiteral("保存拼接图片失败。"));
        return;
    }
    tempFile.close();

    // 导入拼接后的图片
    importFile(tempFile.fileName(), QStringLiteral("拼接乐谱 %1张").arg(images.size()));
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
    QList<Score> scores;
    if (m_filterMode == QStringLiteral("favorites")) {
        scores = m_repository.listFavorites(m_searchQuery, &error);
    } else if (m_filterMode == QStringLiteral("recent")) {
        scores = m_repository.listRecent(m_searchQuery, &error);
    } else if (m_filterMode.startsWith(QStringLiteral("folder:"))) {
        scores = m_repository.listByFolder(m_filterMode.mid(7), m_searchQuery, &error);
    } else if (m_filterMode.startsWith(QStringLiteral("tag:"))) {
        scores = m_repository.listByTag(m_filterMode.mid(4), m_searchQuery, &error);
    } else {
        scores = m_repository.list(m_searchQuery, &error);
    }
    if (!error.isEmpty()) {
        emit errorOccurred(QStringLiteral("加载乐谱库失败。"));
        return;
    }
    m_scores.replaceAll(scores);
}

void LibraryService::reloadFolders()
{
    QString error;
    m_folders.replaceAll(m_repository.folders(&error));
    if (!error.isEmpty()) {
        emit errorOccurred(QStringLiteral("加载文件夹失败。"));
        return;
    }
    emit foldersChanged();
}

void LibraryService::reloadTags()
{
    QString error;
    m_tags.replaceAll(m_repository.tags(&error));
    if (!error.isEmpty()) {
        emit errorOccurred(QStringLiteral("加载标签失败。"));
        return;
    }
    emit tagsChanged();
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
