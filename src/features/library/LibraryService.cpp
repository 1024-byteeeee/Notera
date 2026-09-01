#include "features/library/LibraryService.h"

#include <algorithm>
#include <QBuffer>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QPainter>
#include <QPdfDocument>
#include <QStandardPaths>
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
    , m_entries(this)
    , m_selection(this)
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

LibraryEntryModel* LibraryService::entries()
{
    return &m_entries;
}

LibrarySelectionModel* LibraryService::selection()
{
    return &m_selection;
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
    m_selection.clear();
    m_filterMode = mode;
    if (mode == QStringLiteral("all")) {
        m_currentFolderId.clear();
        m_currentFolderName = QStringLiteral("乐谱库");
        m_currentFolderBreadcrumb = QStringLiteral("乐谱库");
        emit currentFolderChanged();
    } else if (mode.startsWith(QStringLiteral("folder:"))) {
        m_currentFolderId = mode.mid(7);
        QString error;
        m_currentFolderName = m_repository.folderName(m_currentFolderId, &error);
        m_currentFolderBreadcrumb = m_repository.folderBreadcrumb(m_currentFolderId, &error);
        emit currentFolderChanged();
    }
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

QString LibraryService::currentFolderId() const { return m_currentFolderId; }
QString LibraryService::currentFolderName() const { return m_currentFolderName; }
QString LibraryService::currentFolderBreadcrumb() const { return m_currentFolderBreadcrumb; }
bool LibraryService::canGoUp() const { return !m_currentFolderId.isEmpty(); }

void LibraryService::enterFolder(const QString& folderId)
{
    if (folderId.isEmpty()) return;
    QString error;
    const auto name = m_repository.folderName(folderId, &error);
    const auto breadcrumb = m_repository.folderBreadcrumb(folderId, &error);
    if (!error.isEmpty() || name.isEmpty()) {
        emit errorOccurred(QStringLiteral("无法打开文件夹。"));
        return;
    }
    if (!m_repository.markFolderOpened(folderId, &error)) {
        emit errorOccurred(QStringLiteral("无法记录文件夹打开时间。"));
        return;
    }
    m_selection.clear();
    m_currentFolderId = folderId;
    m_currentFolderName = name;
    m_currentFolderBreadcrumb = breadcrumb;
    m_filterMode = QStringLiteral("folder:") + folderId;
    emit currentFolderChanged();
    emit filterModeChanged();
    reload();
}

void LibraryService::markScoreOpened(const QString& scoreId)
{
    if (scoreId.isEmpty()) return;
    QString error;
    if (!m_repository.markScoreOpened(scoreId, &error)) {
        emit errorOccurred(QStringLiteral("无法记录乐谱打开时间。"));
        return;
    }
    if (m_filterMode == QStringLiteral("recent")) reload();
}

void LibraryService::goUp()
{
    if (m_currentFolderId.isEmpty()) return;
    QString error;
    const auto parentId = m_repository.folderParent(m_currentFolderId, &error);
    if (!error.isEmpty()) {
        emit errorOccurred(QStringLiteral("无法返回上一级。"));
        return;
    }
    if (parentId.isEmpty()) {
        goToLibraryRoot();
    } else {
        enterFolder(parentId);
    }
}

void LibraryService::goToLibraryRoot()
{
    m_selection.clear();
    m_currentFolderId.clear();
    m_currentFolderName = QStringLiteral("乐谱库");
    m_currentFolderBreadcrumb = QStringLiteral("乐谱库");
    m_filterMode = QStringLiteral("all");
    emit currentFolderChanged();
    emit filterModeChanged();
    reload();
}

void LibraryService::createFolder(const QString& name)
{
    if (name.trimmed().isEmpty()) {
        emit errorOccurred(QStringLiteral("文件夹名称不能为空。"));
        return;
    }
    QString error;
    const auto parentId = (m_filterMode == QStringLiteral("all")
        || m_filterMode.startsWith(QStringLiteral("folder:"))) ? m_currentFolderId : QString {};
    if (!m_repository.createFolder(name, parentId, &error)) {
        emit errorOccurred(QStringLiteral("创建文件夹失败。"));
        return;
    }
    reloadFolders();
    reload();
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
    if (m_currentFolderId == folderId) {
        m_currentFolderName = name.trimmed();
        QString error;
        m_currentFolderBreadcrumb = m_repository.folderBreadcrumb(folderId, &error);
        emit currentFolderChanged();
    }
    reload();
    emit noticeOccurred(QStringLiteral("已重命名为 %1").arg(name.trimmed()));
}

void LibraryService::deleteFolder(const QString& folderId)
{
    QString error;
    const auto files = m_repository.folderScoresRecursive(folderId, &error);
    if (!error.isEmpty()) {
        emit errorOccurred(QStringLiteral("读取文件夹内容失败。"));
        return;
    }
    for (const auto& value : files) {
        const auto item = value.toMap();
        if (!FileService::removeFile(item.value(QStringLiteral("filePath")).toString(), &error)
            || !FileService::removeFile(item.value(QStringLiteral("thumbnailPath")).toString(), &error)) {
            emit errorOccurred(error);
            return;
        }
    }
    if (!m_repository.deleteFolder(folderId, &error)) {
        emit errorOccurred(QStringLiteral("删除文件夹失败。"));
        return;
    }
    m_currentFolderId.clear();
    m_currentFolderName = QStringLiteral("乐谱库");
    m_currentFolderBreadcrumb = QStringLiteral("乐谱库");
    m_filterMode = QStringLiteral("all");
    emit currentFolderChanged();
    emit filterModeChanged();
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

void LibraryService::importAndStitchImages(const QStringList& filePaths)
{
    if (filePaths.size() < 2) {
        emit errorOccurred(QStringLiteral("拼接导入需要至少选择两张图片。"));
        return;
    }

    // 和 importLocalFile 完全一致的路径解析逻辑
    auto resolveLocalPath = [](const QString& pathOrUrl) -> QString {
        QUrl url(pathOrUrl);
        if (url.isValid() && url.isLocalFile()) {
            return url.toLocalFile();
        }
        if (QFileInfo::exists(pathOrUrl)) {
            return pathOrUrl;
        }
        if (url.scheme().isEmpty()) {
            const QString p = url.path();
            if (!p.isEmpty() && QFileInfo::exists(p)) return p;
        }
        return {};
    };

    // 用 QImageReader 加载（支持更多格式 + 详细错误）
    QList<QImage> images;
    for (const auto& pathOrUrl : filePaths) {
        const QString localPath = resolveLocalPath(pathOrUrl);
        if (localPath.isEmpty()) {
            emit errorOccurred(QStringLiteral("无法读取图片文件：%1").arg(pathOrUrl));
            return;
        }
        QImageReader reader(localPath);
        reader.setAutoTransform(true);
        QImage img = reader.read();
        if (img.isNull()) {
            emit errorOccurred(QStringLiteral("无法加载图片 %1：%2")
                .arg(QFileInfo(localPath).fileName(), reader.errorString()));
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

    // 安全上限
    constexpr qint64 maximumCanvasPixels = 64LL * 1024 * 1024;
    if (totalHeight > 65536 || maxWidth > 16384
        || static_cast<qint64>(maxWidth) * totalHeight > maximumCanvasPixels) {
        emit errorOccurred(QStringLiteral("拼接后图片尺寸过大，请减少图片数量或先缩小图片。"));
        return;
    }

    // 创建拼接画布（白色背景）
    QImage stitched(maxWidth, static_cast<int>(totalHeight), QImage::Format_ARGB32);
    if (stitched.isNull()) {
        emit errorOccurred(QStringLiteral("内存不足，无法创建拼接图片。"));
        return;
    }
    stitched.fill(Qt::white);

    QPainter painter(&stitched);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    int y = 0;
    for (const auto& img : images) {
        int x = (maxWidth - img.width()) / 2;
        painter.drawImage(x, y, img);
        y += img.height();
    }
    painter.end();

    // 保存到临时文件（用固定路径，避免 QTemporaryFile 自动删除）
    const QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    const QString tempPath = QStringLiteral("%1/notera_stitch_%2.png")
        .arg(tempDir, QUuid::createUuid().toString(QUuid::WithoutBraces));
    if (!stitched.save(tempPath, "PNG")) {
        emit errorOccurred(QStringLiteral("保存拼接图片失败。"));
        return;
    }

    // 导入拼接后的图片
    importFile(tempPath, QStringLiteral("拼接乐谱 %1张").arg(images.size()));

    // 清理临时文件
    QFile::remove(tempPath);
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

void LibraryService::toggleItemFavorite(const QString& itemId, const bool favorite)
{
    QString error;
    if (!m_repository.setItemFavorite(itemId, favorite, &error)) {
        emit errorOccurred(QStringLiteral("更新收藏状态失败。"));
        return;
    }
    reloadFolders();
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

void LibraryService::deleteItems(const QVariantList& ids)
{
    QString error;
    int deletedCount = 0;
    for (const auto& idVariant : ids) {
        const auto id = idVariant.toString();
        if (id.isEmpty()) continue;
        const auto type = m_repository.itemTypeById(id, &error);
        if (type == QStringLiteral("score")) {
            const auto filePath = m_repository.filePathById(id, &error);
            const auto thumbPath = m_repository.thumbnailPathById(id, &error);
            if (!FileService::removeFile(filePath, &error)
                || !FileService::removeFile(thumbPath, &error)
                || !m_repository.remove(id, &error)) {
                emit errorOccurred(error.isEmpty() ? QStringLiteral("删除乐谱失败。") : error);
                return;
            }
            ++deletedCount;
        } else if (type == QStringLiteral("folder")) {
            const auto files = m_repository.folderScoresRecursive(id, &error);
            if (!error.isEmpty()) {
                emit errorOccurred(QStringLiteral("读取文件夹内容失败。"));
                return;
            }
            for (const auto& value : files) {
                const auto item = value.toMap();
                if (!FileService::removeFile(item.value(QStringLiteral("filePath")).toString(), &error)
                    || !FileService::removeFile(item.value(QStringLiteral("thumbnailPath")).toString(), &error)) {
                    emit errorOccurred(error);
                    return;
                }
            }
            if (!m_repository.deleteFolder(id, &error)) {
                emit errorOccurred(QStringLiteral("删除文件夹失败。"));
                return;
            }
            ++deletedCount;
        }
    }
    m_selection.clear();
    reloadFolders();
    reload();
    emit noticeOccurred(QStringLiteral("已删除 %1 个项目").arg(deletedCount));
}

QVariantList LibraryService::scoresInFolder(const QString& folderId)
{
    QString error;
    const auto scores = m_repository.listAtFolder(folderId, QString(), &error);
    QVariantList result;
    for (const auto& score : scores) {
        result.append(QVariantMap{
            {"id", score.id},
            {"title", score.title},
            {"filePath", score.filePath},
            {"fileType", score.fileType},
            {"pageCount", score.pageCount},
            {"folderId", folderId}
        });
    }
    return result;
}

QString LibraryService::scoreFolderId(const QString& scoreId)
{
    QString error;
    const auto folderId = m_repository.scoreFolderId(scoreId, &error);
    if (!error.isEmpty()) emit errorOccurred(QStringLiteral("无法确定乐谱所在目录。"));
    return folderId;
}

void LibraryService::setScoreFolder(const QString& scoreId, const QString& folderId)
{
    QString error;
    if (!m_repository.setFolder(scoreId, folderId, &error)) {
        emit errorOccurred(QStringLiteral("设置文件夹失败。"));
        return;
    }
    reload();
    emit noticeOccurred(folderId.isEmpty() ? QStringLiteral("已移出文件夹") : QStringLiteral("已移动到文件夹"));
}

void LibraryService::addScoreTag(const QString& scoreId, const QString& tagId)
{
    QString error;
    if (!m_repository.addTag(scoreId, tagId, &error)) {
        emit errorOccurred(QStringLiteral("添加标签失败。"));
        return;
    }
    reload();
    emit noticeOccurred(QStringLiteral("已添加标签"));
}

void LibraryService::removeScoreTag(const QString& scoreId, const QString& tagId)
{
    QString error;
    if (!m_repository.removeTag(scoreId, tagId, &error)) {
        emit errorOccurred(QStringLiteral("移除标签失败。"));
        return;
    }
    reload();
}

QVariantList LibraryService::scoreTags(const QString& scoreId)
{
    QString error;
    return m_repository.scoreTags(scoreId, &error);
}

bool LibraryService::scoreHasTag(const QString& scoreId, const QString& tagId)
{
    QString error;
    const auto tags = m_repository.scoreTags(scoreId, &error);
    if (!error.isEmpty()) {
        emit errorOccurred(QStringLiteral("加载乐谱标签失败。"));
        return false;
    }
    return std::any_of(tags.cbegin(), tags.cend(), [&tagId](const QVariant& value) {
        return value.toMap().value(QStringLiteral("id")).toString() == tagId;
    });
}

void LibraryService::setItemFolder(const QString& itemId, const QString& folderId)
{
    QString error;
    const auto type = m_repository.itemTypeById(itemId, &error);
    const auto succeeded = type == QStringLiteral("folder")
        ? m_repository.moveFolder(itemId, folderId, &error)
        : type == QStringLiteral("score") && m_repository.setFolder(itemId, folderId, &error);
    if (!succeeded) {
        emit errorOccurred(error.isEmpty() ? QStringLiteral("移动项目失败。") : error);
        return;
    }
    reloadFolders();
    reload();
    emit noticeOccurred(folderId.isEmpty() ? QStringLiteral("已移出文件夹") : QStringLiteral("已移动到文件夹"));
}

void LibraryService::addItemTag(const QString& itemId, const QString& tagId)
{
    QString error;
    if (!m_repository.addItemTag(itemId, tagId, &error)) {
        emit errorOccurred(QStringLiteral("添加标签失败。"));
        return;
    }
    reload();
}

void LibraryService::removeItemTag(const QString& itemId, const QString& tagId)
{
    QString error;
    if (!m_repository.removeItemTag(itemId, tagId, &error)) {
        emit errorOccurred(QStringLiteral("移除标签失败。"));
        return;
    }
    reload();
}

QVariantList LibraryService::itemTags(const QString& itemId)
{
    QString error;
    return m_repository.itemTags(itemId, &error);
}

bool LibraryService::itemHasTag(const QString& itemId, const QString& tagId)
{
    const auto values = itemTags(itemId);
    return std::any_of(values.cbegin(), values.cend(), [&tagId](const QVariant& value) {
        return value.toMap().value(QStringLiteral("id")).toString() == tagId;
    });
}

bool LibraryService::canMoveItemToFolder(const QString& itemId, const QString& folderId)
{
    QString error;
    const auto type = m_repository.itemTypeById(itemId, &error);
    return type == QStringLiteral("score") || (type == QStringLiteral("folder")
        && m_repository.canMoveFolder(itemId, folderId, &error));
}

namespace {
QStringList uniqueItemIds(const QVariantList& values)
{
    QStringList ids;
    for (const auto& value : values) {
        const auto id = value.toString();
        if (!id.isEmpty() && !ids.contains(id)) ids.append(id);
    }
    return ids;
}
}

QString LibraryService::moveItems(const QVariantList& itemIds, const QString& folderId)
{
    const auto ids = uniqueItemIds(itemIds);
    if (ids.isEmpty()) return QStringLiteral("没有可移动的项目。");
    QString error;
    if (!m_repository.moveItems(ids, folderId, &error)) {
        const auto message = error.isEmpty() ? QStringLiteral("移动项目失败。") : error;
        emit errorOccurred(message);
        return message;
    }
    m_selection.clear();
    reloadFolders();
    reload();
    emit noticeOccurred(QStringLiteral("已移动 %1 个项目").arg(ids.size()));
    return {};
}

QString LibraryService::favoriteItems(const QVariantList& itemIds)
{
    const auto ids = uniqueItemIds(itemIds);
    if (ids.isEmpty()) return QStringLiteral("没有可收藏的项目。");
    QString error;
    if (!m_repository.setItemsFavorite(ids, true, &error)) {
        emit errorOccurred(QStringLiteral("添加收藏失败。"));
        return QStringLiteral("添加收藏失败。");
    }
    reloadFolders();
    reload();
    emit noticeOccurred(QStringLiteral("已收藏 %1 个项目").arg(ids.size()));
    return {};
}

QString LibraryService::tagItems(const QVariantList& itemIds, const QString& tagId)
{
    const auto ids = uniqueItemIds(itemIds);
    if (ids.isEmpty() || tagId.isEmpty()) return QStringLiteral("没有可添加标签的项目。");
    QString error;
    if (!m_repository.addItemsTag(ids, tagId, &error)) {
        emit errorOccurred(QStringLiteral("添加标签失败。"));
        return QStringLiteral("添加标签失败。");
    }
    reload();
    emit noticeOccurred(QStringLiteral("已为 %1 个项目添加标签").arg(ids.size()));
    return {};
}

void LibraryService::reload()
{
    QString error;
    QList<Score> scores;
    QVariantList visibleFolders;
    if (m_filterMode == QStringLiteral("favorites")) {
        scores = m_repository.listFavorites(m_searchQuery, &error);
        visibleFolders = m_repository.favoriteFolders(m_searchQuery, &error);
    } else if (m_filterMode == QStringLiteral("recent")) {
        scores = m_repository.listRecent(m_searchQuery, &error);
        visibleFolders = m_repository.recentFolders(m_searchQuery, &error);
    } else if (m_filterMode.startsWith(QStringLiteral("folder:"))) {
        scores = m_repository.listAtFolder(m_currentFolderId, m_searchQuery, &error);
        visibleFolders = m_repository.childFolders(m_currentFolderId, &error);
    } else if (m_filterMode.startsWith(QStringLiteral("tag:"))) {
        const auto tagId = m_filterMode.mid(4);
        scores = m_repository.listByTag(tagId, m_searchQuery, &error);
        visibleFolders = m_repository.foldersByTag(tagId, m_searchQuery, &error);
    } else {
        scores = m_repository.listAtFolder({}, m_searchQuery, &error);
        visibleFolders = m_repository.childFolders({}, &error);
    }
    if (!error.isEmpty()) {
        emit errorOccurred(QStringLiteral("加载乐谱库失败。"));
        return;
    }
    for (auto& score : scores) {
        const auto values = m_repository.scoreTags(score.id, &error);
        for (const auto& value : values) score.tags.append(value.toMap().value(QStringLiteral("name")).toString());
    }
    m_scores.replaceAll(scores);
    m_entries.replaceAll(visibleFolders, scores);
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
    const auto destinationFolder = (m_filterMode == QStringLiteral("all")
        || m_filterMode.startsWith(QStringLiteral("folder:"))) ? m_currentFolderId : QString {};
    if (!m_repository.insert(score, destinationFolder, &error)) {
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
