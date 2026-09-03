#include "features/library/LibraryService.h"

#include <algorithm>
#include <functional>
#include <QBuffer>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QPdfDocument>
#include <QRegularExpression>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QUrl>
#include <QUuid>
#include <QtCore/private/qzipreader_p.h>

#include "platform/AppDataPaths.h"
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

QString safeExportName(QString name)
{
    name = name.trimmed();
    name.replace(QRegularExpression(QStringLiteral(R"([<>:"/\\|?*\x00-\x1F])")), QStringLiteral("_"));
    while (name.endsWith(QLatin1Char('.')) || name.endsWith(QLatin1Char(' '))) name.chop(1);
    return name.isEmpty() ? QStringLiteral("未命名") : name;
}

QString availableExportPath(const QString& requestedPath)
{
    if (!QFileInfo::exists(requestedPath)) return requestedPath;
    const QFileInfo info(requestedPath);
    const auto suffix = info.completeSuffix();
    const auto base = suffix.isEmpty() ? info.fileName()
        : info.fileName().left(info.fileName().size() - suffix.size() - 1);
    for (int index = 2; ; ++index) {
        const auto fileName = suffix.isEmpty() ? QStringLiteral("%1 (%2)").arg(base).arg(index)
            : QStringLiteral("%1 (%2).%3").arg(base).arg(index).arg(suffix);
        const auto candidate = info.dir().filePath(fileName);
        if (!QFileInfo::exists(candidate)) return candidate;
    }
}

bool unzipBackupToDirectory(const QString& zipPath, const QString& dstDir, QString* error)
{
    QZipReader reader(zipPath);
    if (!reader.exists()) {
        *error = QStringLiteral("无法打开备份压缩包。");
        return false;
    }
    const auto entries = reader.fileInfoList();
    for (const auto& entry : entries) {
        const auto targetPath = QDir(dstDir).filePath(entry.filePath);
        if (entry.isDir) {
            if (!QDir().mkpath(targetPath)) {
                *error = QStringLiteral("无法创建目录：%1").arg(targetPath);
                return false;
            }
        } else {
            if (!QDir().mkpath(QFileInfo(targetPath).absolutePath())) {
                *error = QStringLiteral("无法创建目录：%1").arg(QFileInfo(targetPath).absolutePath());
                return false;
            }
            QFile file(targetPath);
            if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                *error = QStringLiteral("无法写入文件：%1").arg(targetPath);
                return false;
            }
            file.write(reader.fileData(entry.filePath));
            file.close();
        }
    }
    reader.close();
    return true;
}

bool readBackupManifest(const QString& backupRoot, QJsonObject* manifest, QString* error)
{
    QFile file(backupRoot + QStringLiteral("/manifest.json"));
    if (!file.open(QIODevice::ReadOnly)) {
        *error = QStringLiteral("所选文件不是 Notera 数据库备份。");
        return false;
    }
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        *error = QStringLiteral("备份清单已损坏。");
        return false;
    }
    *manifest = document.object();
    if (manifest->value(QStringLiteral("format")).toString() != QStringLiteral("notera-backup")
        || manifest->value(QStringLiteral("formatVersion")).toInt() != 1
        || !QFileInfo::exists(backupRoot + QStringLiteral("/database/notera.db"))) {
        *error = QStringLiteral("备份格式不受支持或数据库文件缺失。");
        return false;
    }
    return true;
}

bool validateBackupDatabase(const QString& databasePath, QString* error)
{
    const auto connectionName = QStringLiteral("notera_merge_validation_")
        + QUuid::createUuid().toString(QUuid::WithoutBraces);
    bool valid = false;
    {
        auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        database.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
        database.setDatabaseName(databasePath);
        if (database.open()) {
            QSqlQuery query(database);
            valid = query.exec(QStringLiteral("PRAGMA integrity_check")) && query.next()
                && query.value(0).toString() == QStringLiteral("ok");
        }
        if (!valid) *error = QStringLiteral("备份数据库完整性校验失败。");
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
    return valid;
}

}

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

    constexpr qint64 maximumCanvasPixels = 64LL * 1024 * 1024;
    if (totalHeight > 65536 || maxWidth > 16384
        || static_cast<qint64>(maxWidth) * totalHeight > maximumCanvasPixels) {
        emit errorOccurred(QStringLiteral("拼接后图片尺寸过大，请减少图片数量或先缩小图片。"));
        return;
    }

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

    const QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    const QString tempPath = QStringLiteral("%1/notera_stitch_%2.png")
        .arg(tempDir, QUuid::createUuid().toString(QUuid::WithoutBraces));
    if (!stitched.save(tempPath, "PNG")) {
        emit errorOccurred(QStringLiteral("保存拼接图片失败。"));
        return;
    }

    importFile(tempPath, QStringLiteral("拼接乐谱 %1张").arg(images.size()));

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
    const auto currentFolderId = m_repository.scoreFolderId(scoreId, &error);
    if (!error.isEmpty()) {
        emit errorOccurred(QStringLiteral("无法确定乐谱所在目录。"));
        return;
    }
    if (currentFolderId == folderId) return;
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
    const auto currentFolderId = type == QStringLiteral("folder")
        ? m_repository.folderParent(itemId, &error)
        : type == QStringLiteral("score") ? m_repository.scoreFolderId(itemId, &error) : QString {};
    if (!error.isEmpty()) {
        emit errorOccurred(error);
        return;
    }
    if (!type.isEmpty() && currentFolderId == folderId) return;
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
    int changedCount = 0;
    if (!m_repository.moveItems(ids, folderId, &changedCount, &error)) {
        const auto message = error.isEmpty() ? QStringLiteral("移动项目失败。") : error;
        emit errorOccurred(message);
        return message;
    }
    if (changedCount == 0) return {};
    m_selection.clear();
    reloadFolders();
    reload();
    emit noticeOccurred(QStringLiteral("已移动 %1 个项目").arg(changedCount));
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

QString LibraryService::saveScoreAs(const QString& scoreId, const QUrl& destination)
{
    if (!destination.isValid() || !destination.isLocalFile()) return QStringLiteral("请选择本机保存位置。");
    QString error;
    const auto sourcePath = m_repository.filePathById(scoreId, &error);
    if (sourcePath.isEmpty() || !QFileInfo::exists(sourcePath)) return QStringLiteral("找不到乐谱源文件。");
    auto destinationPath = QDir::cleanPath(destination.toLocalFile());
    if (QFileInfo(destinationPath).suffix().isEmpty()) {
        destinationPath += QLatin1Char('.') + QFileInfo(sourcePath).suffix();
    }
    if (QFileInfo(sourcePath).canonicalFilePath() == QFileInfo(destinationPath).canonicalFilePath()) {
        return QStringLiteral("保存位置与源文件相同。");
    }
    if (QFileInfo::exists(destinationPath) && !QFile::remove(destinationPath)) {
        return QStringLiteral("无法覆盖目标文件。");
    }
    if (!QFile::copy(sourcePath, destinationPath)) return QStringLiteral("另存乐谱失败。");
    emit noticeOccurred(QStringLiteral("乐谱已另存为"));
    return {};
}

QString LibraryService::saveFolderAs(const QString& folderId, const QUrl& destinationDirectory)
{
    if (!destinationDirectory.isValid() || !destinationDirectory.isLocalFile()) {
        return QStringLiteral("请选择本机文件夹。");
    }
    QString error;
    const auto entries = m_repository.folderExportEntries(folderId, &error);
    if (!error.isEmpty() || entries.isEmpty()) return QStringLiteral("无法读取文件夹内容。");
    const auto destinationRoot = QDir::cleanPath(destinationDirectory.toLocalFile());
    int copied = 0;
    for (const auto& value : entries) {
        const auto entry = value.toMap();
        const auto rawSegments = entry.value(QStringLiteral("relativePath")).toString().split(QLatin1Char('/'), Qt::SkipEmptyParts);
        QStringList safeSegments;
        for (const auto& segment : rawSegments) safeSegments.append(safeExportName(segment));
        const auto folderPath = QDir(destinationRoot).filePath(safeSegments.join(QLatin1Char('/')));
        if (!QDir().mkpath(folderPath)) return QStringLiteral("无法创建导出文件夹。");
        const auto sourcePath = entry.value(QStringLiteral("filePath")).toString();
        if (sourcePath.isEmpty()) continue;
        const auto suffix = QFileInfo(sourcePath).suffix();
        auto fileName = safeExportName(entry.value(QStringLiteral("title")).toString());
        if (!suffix.isEmpty()) fileName += QLatin1Char('.') + suffix;
        const auto targetPath = availableExportPath(QDir(folderPath).filePath(fileName));
        if (!QFile::copy(sourcePath, targetPath)) return QStringLiteral("导出文件夹时复制乐谱失败。");
        ++copied;
    }
    emit noticeOccurred(QStringLiteral("文件夹已另存，导出 %1 份乐谱").arg(copied));
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
        (void)FileService::removeFile(storedPath, &error); // 回滚：尽力删除已复制文件
        emit errorOccurred(QStringLiteral("将乐谱添加到乐谱库失败。"));
        return;
    }
    if (FileService::isSupportedScoreFile(score.filePath)) {
        m_thumbnailGenerator.generate(score.id, score.filePath, score.fileType);
    }
    reload();
    emit noticeOccurred(QStringLiteral("已导入 %1").arg(score.title));
}

QVariantList LibraryService::childFolders(const QString& folderId)
{
    QString error;
    return m_repository.childFolders(folderId, &error);
}

QVariantList LibraryService::clipboardItems() const { return m_clipboardItems; }
QString LibraryService::clipboardMode() const { return m_clipboardMode; }

void LibraryService::copyItems(const QVariantList& itemIds)
{
    m_clipboardItems = itemIds;
    m_clipboardMode = QStringLiteral("copy");
    emit clipboardChanged();
}

void LibraryService::cutItems(const QVariantList& itemIds)
{
    m_clipboardItems = itemIds;
    m_clipboardMode = QStringLiteral("cut");
    emit clipboardChanged();
}

void LibraryService::clearClipboard()
{
    m_clipboardItems.clear();
    m_clipboardMode = QStringLiteral("none");
    emit clipboardChanged();
}

bool LibraryService::nameExistsInFolder(const QString& name, const QString& folderId, bool isFolder)
{
    QString error;
    if (isFolder) {
        const auto folders = m_repository.childFolders(folderId, &error);
        for (const auto& f : folders) {
            if (f.toMap().value(QStringLiteral("name")).toString() == name) return true;
        }
        return false;
    }
    const auto scores = m_repository.listAtFolder(folderId, QString(), &error);
    for (const auto& s : scores) {
        if (s.title == name) return true;
    }
    return false;
}

QString LibraryService::uniqueNameInFolder(const QString& baseName, const QString& folderId, bool isFolder)
{
    if (!nameExistsInFolder(baseName, folderId, isFolder)) return baseName;
    int n = 2;
    while (true) {
        const auto candidate = QStringLiteral("%1 (%2)").arg(baseName).arg(n);
        if (!nameExistsInFolder(candidate, folderId, isFolder)) return candidate;
        ++n;
    }
}

QString LibraryService::copyScoreToFolder(const QString& scoreId, const QString& targetFolderId, const QString& conflictAction)
{
    QString error;
    const auto sourcePath = m_repository.filePathById(scoreId, &error);
    if (sourcePath.isEmpty() || !QFileInfo::exists(sourcePath)) return QStringLiteral("找不到源乐谱文件。");

    const auto scores = m_repository.listAtFolder(targetFolderId, QString(), &error);
    QString sourceTitle;
    for (const auto& s : scores) {
        if (s.id == scoreId) { sourceTitle = s.title; break; }
    }
    if (sourceTitle.isEmpty()) {
        const auto all = m_repository.list(QString(), &error);
        for (const auto& s : all) {
            if (s.id == scoreId) { sourceTitle = s.title; break; }
        }
    }

    QString targetTitle = sourceTitle;
    if (nameExistsInFolder(sourceTitle, targetFolderId, false)) {
        if (conflictAction == QStringLiteral("skip")) return {};
        if (conflictAction == QStringLiteral("rename")) {
            targetTitle = uniqueNameInFolder(sourceTitle, targetFolderId, false);
        } else if (conflictAction == QStringLiteral("overwrite")) {
            for (const auto& s : scores) {
                if (s.title == sourceTitle) {
                    if (!m_repository.remove(s.id, &error)) {
                        return error.isEmpty() ? QStringLiteral("移除旧乐谱失败。") : error;
                    }
                    (void)FileService::removeFile(m_repository.filePathById(s.id, &error), &error);
                    break;
                }
            }
        }
    }

    const auto newId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const auto storedPath = FileService::copyScoreIntoLibrary(sourcePath, newId, &error);
    if (storedPath.isEmpty()) return error.isEmpty() ? QStringLiteral("复制文件失败。") : error;

    const auto now = QDateTime::currentDateTimeUtc();
    Score score {
        .id = newId,
        .title = targetTitle,
        .fileName = QFileInfo(storedPath).fileName(),
        .filePath = storedPath,
        .fileType = FileService::canonicalSuffix(sourcePath),
        .pageCount = pageCountForFile(storedPath, FileService::canonicalSuffix(sourcePath)),
        .createdAt = now,
        .updatedAt = now
    };
    if (!m_repository.insert(score, targetFolderId, &error)) {
        (void)FileService::removeFile(storedPath, &error); // 回滚：尽力删除已复制文件
        return QStringLiteral("创建乐谱记录失败。");
    }
    if (FileService::isSupportedScoreFile(score.filePath)) {
        m_thumbnailGenerator.generate(score.id, score.filePath, score.fileType);
    }
    return {};
}

QString LibraryService::copyFolderRecursive(const QString& folderId, const QString& targetParentId, const QString& conflictAction)
{
    QString error;
    const auto sourceName = m_repository.folderName(folderId, &error);
    if (sourceName.isEmpty()) return QStringLiteral("找不到源文件夹。");

    QString targetName = sourceName;
    if (nameExistsInFolder(sourceName, targetParentId, true)) {
        if (conflictAction == QStringLiteral("skip")) return {};
        if (conflictAction == QStringLiteral("rename")) {
            targetName = uniqueNameInFolder(sourceName, targetParentId, true);
        } else if (conflictAction == QStringLiteral("overwrite")) {
            const auto children = m_repository.childFolders(targetParentId, &error);
            for (const auto& f : children) {
                if (f.toMap().value(QStringLiteral("name")).toString() == sourceName) {
                    if (!m_repository.deleteFolder(f.toMap().value(QStringLiteral("itemId")).toString(), &error)) {
                        return error.isEmpty() ? QStringLiteral("移除旧文件夹失败。") : error;
                    }
                    break;
                }
            }
        }
    }

    const auto newFolderId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (!m_repository.createFolder(targetName, targetParentId, &error)) {
        return QStringLiteral("创建文件夹失败。");
    }

    const auto childScores = m_repository.listAtFolder(folderId, QString(), &error);
    for (const auto& s : childScores) {
        copyScoreToFolder(s.id, newFolderId, conflictAction);
    }

    const auto childFolders = m_repository.childFolders(folderId, &error);
    for (const auto& f : childFolders) {
        copyFolderRecursive(f.toMap().value(QStringLiteral("itemId")).toString(), newFolderId, conflictAction);
    }
    return {};
}

QString LibraryService::getOrCreateFolder(const QString& name, const QString& parentId)
{
    QString error;
    const auto children = m_repository.childFolders(parentId, &error);
    for (const auto& f : children) {
        if (f.toMap().value(QStringLiteral("name")).toString() == name) {
            return f.toMap().value(QStringLiteral("id")).toString();
        }
    }
    // createFolder 内部自己生成 UUID，创建后必须重新查询获取真实 id
    if (!m_repository.createFolder(name, parentId, &error)) {
        emit errorOccurred(QStringLiteral("创建文件夹失败：%1").arg(name));
        return {};
    }
    const auto updated = m_repository.childFolders(parentId, &error);
    for (const auto& f : updated) {
        if (f.toMap().value(QStringLiteral("name")).toString() == name) {
            return f.toMap().value(QStringLiteral("id")).toString();
        }
    }
    return {};
}

void LibraryService::expandFolderToQueue(const QString& sourceFolderId, const QString& targetFolderId)
{
    QString error;
    int insertOffset = 0;

    // 展开内部乐谱：插入到当前位置之后，逐个进入冲突处理流程
    const auto scores = m_repository.listAtFolder(sourceFolderId, QString(), &error);
    for (const auto& s : scores) {
        m_pasteQueue.insert(m_pasteIndex + 1 + insertOffset, QVariantMap{
            {QStringLiteral("itemId"), s.id},
            {QStringLiteral("targetFolderId"), targetFolderId}
        });
        ++insertOffset;
    }

    // 展开子文件夹：targetFolderId 为父目标，continuePaste 会自动合并同名子文件夹并递归展开
    const auto subFolders = m_repository.childFolders(sourceFolderId, &error);
    for (const auto& f : subFolders) {
        m_pasteQueue.insert(m_pasteIndex + 1 + insertOffset, QVariantMap{
            {QStringLiteral("itemId"), f.toMap().value(QStringLiteral("id")).toString()},
            {QStringLiteral("targetFolderId"), targetFolderId}
        });
        ++insertOffset;
    }
}

void LibraryService::deleteEmptyFolderTree(const QString& folderId)
{
    QString error;
    // 先递归清理空子文件夹
    const auto subFolders = m_repository.childFolders(folderId, &error);
    for (const auto& f : subFolders) {
        deleteEmptyFolderTree(f.toMap().value(QStringLiteral("id")).toString());
    }
    // 仅当文件夹内已无乐谱时才删除（被跳过的文件应保留在原位置）
    const auto scores = m_repository.listAtFolder(folderId, QString(), &error);
    if (scores.isEmpty()) {
        if (!m_repository.deleteFolder(folderId, &error)) {
            emit errorOccurred(QStringLiteral("清理空文件夹失败。"));
        }
    }
}

void LibraryService::pasteItems()
{
    if (m_clipboardItems.isEmpty() || m_clipboardMode == QStringLiteral("none")) return;
    m_pasteQueue.clear();
    m_cutSourceFolderIds.clear();
    const auto isCut = m_clipboardMode == QStringLiteral("cut");
    QString error;
    for (const auto& item : m_clipboardItems) {
        const auto itemId = item.toString();
        m_pasteQueue.append(QVariantMap{
            {QStringLiteral("itemId"), itemId},
            {QStringLiteral("targetFolderId"), m_currentFolderId}
        });
        if (isCut && m_repository.itemTypeById(itemId, &error) == QStringLiteral("folder")) {
            m_cutSourceFolderIds.append(itemId);
        }
    }
    m_pasteIndex = 0;
    m_pasteTargetFolderId = m_currentFolderId;
    m_pendingConflictAction.clear();
    m_pasteApplyToAll = false;
    continuePaste();
}

void LibraryService::resolvePasteConflict(const QString& action, bool applyToAll)
{
    m_pendingConflictAction = action;
    m_pasteApplyToAll = applyToAll;
    if (action == QStringLiteral("cancel")) {
        m_pasteQueue.clear();
        m_pasteIndex = 0;
        reloadFolders();
        reload();
        emit pasteFinished(0);
        return;
    }
    continuePaste();
}

void LibraryService::continuePaste()
{
    QString error;
    int processed = 0;
    const auto isCut = m_clipboardMode == QStringLiteral("cut");

    while (m_pasteIndex < m_pasteQueue.size()) {
        const auto item = m_pasteQueue[m_pasteIndex].toMap();
        const auto itemId = item.value(QStringLiteral("itemId")).toString();
        const auto targetFolderId = item.value(QStringLiteral("targetFolderId")).toString();
        const auto type = m_repository.itemTypeById(itemId, &error);
        if (type.isEmpty()) { ++m_pasteIndex; continue; }

        const bool isFolder = type == QStringLiteral("folder");

        // 剪切到同一目录：跳过
        const auto currentParent = isFolder
            ? m_repository.folderParent(itemId, &error)
            : m_repository.scoreFolderId(itemId, &error);
        if (currentParent == targetFolderId && isCut) {
            ++m_pasteIndex;
            continue;
        }

        // 文件夹：同名自动合并（不弹窗），递归展开内部内容到队列逐个处理
        if (isFolder) {
            const auto sourceName = m_repository.folderName(itemId, &error);
            if (sourceName.isEmpty()) { ++m_pasteIndex; continue; }
            const auto mergedFolderId = getOrCreateFolder(sourceName, targetFolderId);
            expandFolderToQueue(itemId, mergedFolderId);
            ++m_pasteIndex;
            continue;
        }

        // 乐谱：逐个冲突判断
        QString itemName;
        {
            const auto all = m_repository.list(QString(), &error);
            for (const auto& s : all) {
                if (s.id == itemId) { itemName = s.title; break; }
            }
        }
        if (itemName.isEmpty()) { ++m_pasteIndex; continue; }

        const bool hasConflict = nameExistsInFolder(itemName, targetFolderId, false);
        QString action = m_pendingConflictAction;
        if (hasConflict && action.isEmpty()) {
            emit pasteConflict(itemName, itemName, m_pasteIndex, m_pasteQueue.size());
            return;
        }
        if (hasConflict && action.isEmpty()) action = QStringLiteral("rename");

        if (isCut) {
            if (hasConflict) {
                if (action == QStringLiteral("skip")) {
                    ++m_pasteIndex;
                    if (!m_pasteApplyToAll) m_pendingConflictAction.clear();
                    continue;
                }
                if (action == QStringLiteral("overwrite")) {
                    const auto scores = m_repository.listAtFolder(targetFolderId, QString(), &error);
                    for (const auto& s : scores) {
                        if (s.title == itemName) {
                            if (m_repository.remove(s.id, &error)) {
                                (void)FileService::removeFile(m_repository.filePathById(s.id, &error), &error);
                            } else {
                                emit errorOccurred(QStringLiteral("移除旧乐谱失败。"));
                            }
                            break;
                        }
                    }
                }
                // rename：移动后给乐谱改名
            }
            if (!m_repository.setFolder(itemId, targetFolderId, &error)) {
                emit errorOccurred(QStringLiteral("移动乐谱失败。"));
            } else if (hasConflict && action == QStringLiteral("rename")) {
                if (!m_repository.rename(itemId, uniqueNameInFolder(itemName, targetFolderId, false), &error)) {
                    emit errorOccurred(QStringLiteral("重命名乐谱失败。"));
                }
            }
        } else {
            (void)copyScoreToFolder(itemId, targetFolderId, action);
        }
        ++processed;
        ++m_pasteIndex;
        if (!m_pasteApplyToAll) m_pendingConflictAction.clear();
    }

    // 剪切模式：只删除已变空的源文件夹结构；被跳过的文件保留在原位置（对齐 Windows）
    if (isCut) {
        for (const auto& folderId : m_cutSourceFolderIds) {
            deleteEmptyFolderTree(folderId);
        }
        m_cutSourceFolderIds.clear();
        clearClipboard();
    }
    reloadFolders();
    reload();
    if (processed > 0) {
        emit noticeOccurred(isCut
            ? QStringLiteral("已移动 %1 个项目").arg(processed)
            : QStringLiteral("已复制 %1 个项目").arg(processed));
    }
    emit pasteFinished(processed);
}

QString LibraryService::sha256OfFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(&file);
    return QString::fromLatin1(hash.result().toHex());
}

void LibraryService::cleanupMergeState()
{
    m_mergeTempDir.reset();
    m_mergeBackupRoot.clear();
    m_mergeQueue.clear();
    m_mergeIndex = 0;
    m_mergeConflictAction.clear();
    m_mergeApplyToAll = false;
    m_mergeFolderMap.clear();
    m_mergeTagMap.clear();
    m_mergeHashIndex.clear();
    m_mergeScoreTitles.clear();
}

QVariantMap LibraryService::probeDatabaseBackup(const QUrl& backupFile)
{
    QVariantMap result;
    result[QStringLiteral("valid")] = false;
    if (!backupFile.isValid() || !backupFile.isLocalFile()) {
        result[QStringLiteral("error")] = QStringLiteral("请选择本机备份文件。");
        return result;
    }
    const auto zipPath = QDir::cleanPath(backupFile.toLocalFile());
    if (!QFileInfo::exists(zipPath)) {
        result[QStringLiteral("error")] = QStringLiteral("备份文件不存在。");
        return result;
    }
    QTemporaryDir tempDir;
    if (!tempDir.isValid()) {
        result[QStringLiteral("error")] = QStringLiteral("无法创建临时目录。");
        return result;
    }
    QString error;
    if (!unzipBackupToDirectory(zipPath, tempDir.path(), &error)) {
        result[QStringLiteral("error")] = error;
        return result;
    }
    QJsonObject manifest;
    if (!readBackupManifest(tempDir.path(), &manifest, &error)) {
        result[QStringLiteral("error")] = error;
        return result;
    }
    const auto databasePath = tempDir.path() + QStringLiteral("/database/notera.db");
    if (!validateBackupDatabase(databasePath, &error)) {
        result[QStringLiteral("error")] = error;
        return result;
    }
    const auto connectionName = QStringLiteral("notera_merge_probe_")
        + QUuid::createUuid().toString(QUuid::WithoutBraces);
    {
        auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        database.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
        database.setDatabaseName(databasePath);
        if (database.open()) {
            QSqlQuery query(database);
            if (query.exec(QStringLiteral("SELECT COUNT(*) FROM scores")) && query.next())
                result[QStringLiteral("scoreCount")] = query.value(0).toInt();
            if (query.exec(QStringLiteral("SELECT COUNT(*) FROM folders")) && query.next())
                result[QStringLiteral("folderCount")] = query.value(0).toInt();
            if (query.exec(QStringLiteral("SELECT COUNT(*) FROM tags")) && query.next())
                result[QStringLiteral("tagCount")] = query.value(0).toInt();
        }
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
    result[QStringLiteral("valid")] = true;
    result[QStringLiteral("createdAt")] = manifest.value(QStringLiteral("createdAt")).toString();
    result[QStringLiteral("applicationVersion")] = manifest.value(QStringLiteral("applicationVersion")).toString();
    return result;
}

QString LibraryService::importDatabaseBackupMerged(const QUrl& backupFile)
{
    if (!backupFile.isValid() || !backupFile.isLocalFile()) return QStringLiteral("请选择本机备份文件。");
    const auto zipPath = QDir::cleanPath(backupFile.toLocalFile());
    if (!QFileInfo::exists(zipPath)) return QStringLiteral("备份文件不存在。");

    cleanupMergeState();
    m_mergeTempDir.reset(new QTemporaryDir);
    if (!m_mergeTempDir->isValid()) return QStringLiteral("无法创建临时目录。");
    m_mergeBackupRoot = m_mergeTempDir->path();

    QString error;
    if (!unzipBackupToDirectory(zipPath, m_mergeBackupRoot, &error)) return error;
    QJsonObject manifest;
    if (!readBackupManifest(m_mergeBackupRoot, &manifest, &error)) return error;
    if (!validateBackupDatabase(m_mergeBackupRoot + QStringLiteral("/database/notera.db"), &error)) return error;

    const auto databasePath = m_mergeBackupRoot + QStringLiteral("/database/notera.db");
    const auto connectionName = QStringLiteral("notera_merge_backup_")
        + QUuid::createUuid().toString(QUuid::WithoutBraces);

    QVariantList backupTags;
    QVariantList backupFolders;
    QVariantList backupScores;
    QHash<QString, QList<QString>> tagsByFolder;
    QHash<QString, QList<QString>> tagsByScore;
    QHash<QString, QVariantList> annotationsByScore;

    {
        auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        database.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
        database.setDatabaseName(databasePath);
        if (!database.open()) {
            QSqlDatabase::removeDatabase(connectionName);
            cleanupMergeState();
            return QStringLiteral("无法读取备份数据库。");
        }
        QSqlQuery query(database);

        if (query.exec(QStringLiteral("SELECT id, name FROM tags"))) {
            while (query.next()) {
                backupTags.append(QVariantMap{
                    {QStringLiteral("id"), query.value(0).toString()},
                    {QStringLiteral("name"), query.value(1).toString()}
                });
            }
        }
        if (query.exec(QStringLiteral("SELECT id, name, parent_id, favorite, created_at, updated_at FROM folders"))) {
            while (query.next()) {
                const auto parent = query.value(2);
                backupFolders.append(QVariantMap{
                    {QStringLiteral("id"), query.value(0).toString()},
                    {QStringLiteral("name"), query.value(1).toString()},
                    {QStringLiteral("parentId"), parent.isNull() ? QString() : parent.toString()},
                    {QStringLiteral("favorite"), query.value(3).toInt() != 0},
                    {QStringLiteral("createdAt"), query.value(4).toLongLong()},
                    {QStringLiteral("updatedAt"), query.value(5).toLongLong()}
                });
            }
        }
        if (query.exec(QStringLiteral("SELECT folder_id, tag_id FROM folder_tags"))) {
            while (query.next()) {
                tagsByFolder[query.value(0).toString()].append(query.value(1).toString());
            }
        }
        if (query.exec(QStringLiteral("SELECT id, title, composer, file_name, file_path, file_type, page_count, favorite, last_page, created_at, updated_at, last_opened_at, folder_id FROM scores"))) {
            while (query.next()) {
                const auto lastOpened = query.value(11);
                const auto folder = query.value(12);
                backupScores.append(QVariantMap{
                    {QStringLiteral("id"), query.value(0).toString()},
                    {QStringLiteral("title"), query.value(1).toString()},
                    {QStringLiteral("composer"), query.value(2).toString()},
                    {QStringLiteral("fileName"), query.value(3).toString()},
                    {QStringLiteral("filePath"), query.value(4).toString()},
                    {QStringLiteral("fileType"), query.value(5).toString()},
                    {QStringLiteral("pageCount"), query.value(6).toInt()},
                    {QStringLiteral("favorite"), query.value(7).toInt() != 0},
                    {QStringLiteral("lastPage"), query.value(8).toInt()},
                    {QStringLiteral("createdAt"), query.value(9).toLongLong()},
                    {QStringLiteral("updatedAt"), query.value(10).toLongLong()},
                    {QStringLiteral("lastOpenedAt"), lastOpened.isNull() ? QVariant(qint64(0)) : QVariant(lastOpened.toLongLong())},
                    {QStringLiteral("folderId"), folder.isNull() ? QString() : folder.toString()}
                });
            }
        }
        if (query.exec(QStringLiteral("SELECT score_id, tag_id FROM score_tags"))) {
            while (query.next()) {
                tagsByScore[query.value(0).toString()].append(query.value(1).toString());
            }
        }
        if (query.exec(QStringLiteral("SELECT score_id, page, type, data, created_at, updated_at FROM annotations"))) {
            while (query.next()) {
                annotationsByScore[query.value(0).toString()].append(QVariantMap{
                    {QStringLiteral("page"), query.value(1).toInt()},
                    {QStringLiteral("type"), query.value(2).toString()},
                    {QStringLiteral("data"), query.value(3).toString()},
                    {QStringLiteral("createdAt"), query.value(4).toLongLong()},
                    {QStringLiteral("updatedAt"), query.value(5).toLongLong()}
                });
            }
        }
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);

    // 1. 合并标签（按名称去重）
    QHash<QString, QString> currentTagNameToId;
    {
        const auto currentTags = m_repository.tags(&error);
        for (const auto& tag : currentTags) {
            currentTagNameToId[tag.toMap().value(QStringLiteral("name")).toString()]
                = tag.toMap().value(QStringLiteral("id")).toString();
        }
    }
    {
        QSqlQuery insertTag(m_databaseService.database());
        for (const auto& value : backupTags) {
            const auto map = value.toMap();
            const auto oldId = map.value(QStringLiteral("id")).toString();
            const auto name = map.value(QStringLiteral("name")).toString();
            if (currentTagNameToId.contains(name)) {
                m_mergeTagMap[oldId] = currentTagNameToId.value(name);
                continue;
            }
            const auto newId = QUuid::createUuid().toString(QUuid::WithoutBraces);
            insertTag.prepare(QStringLiteral("INSERT INTO tags (id, name) VALUES (?, ?)"));
            insertTag.addBindValue(newId);
            insertTag.addBindValue(name);
            if (insertTag.exec()) {
                m_mergeTagMap[oldId] = newId;
                currentTagNameToId[name] = newId;
            }
        }
    }

    // 2. 合并文件夹：同名按层级向下合并（不弹窗），不存在则新建
    {
        std::function<void(const QString&, const QString&)> mergeFolderLevel =
            [&](const QString& backupParentId, const QString& targetParentId) {
            for (const auto& value : backupFolders) {
                const auto map = value.toMap();
                if (map.value(QStringLiteral("parentId")).toString() != backupParentId) continue;
                const auto name = map.value(QStringLiteral("name")).toString();
                QString matchedId;
                const auto children = m_repository.childFolders(targetParentId, &error);
                for (const auto& child : children) {
                    if (child.toMap().value(QStringLiteral("name")).toString() == name) {
                        matchedId = child.toMap().value(QStringLiteral("id")).toString();
                        break;
                    }
                }
                const auto oldId = map.value(QStringLiteral("id")).toString();
                QString newId = matchedId;
                if (newId.isEmpty()) {
                    newId = QUuid::createUuid().toString(QUuid::WithoutBraces);
                    QSqlQuery insertFolder(m_databaseService.database());
                    insertFolder.prepare(QStringLiteral(
                        "INSERT INTO folders (id, name, created_at, updated_at, parent_id, favorite) VALUES (?, ?, ?, ?, ?, ?)"));
                    insertFolder.addBindValue(newId);
                    insertFolder.addBindValue(name);
                    insertFolder.addBindValue(map.value(QStringLiteral("createdAt")).toLongLong());
                    insertFolder.addBindValue(map.value(QStringLiteral("updatedAt")).toLongLong());
                    insertFolder.addBindValue(targetParentId.isEmpty() ? QVariant() : QVariant(targetParentId));
                    insertFolder.addBindValue(map.value(QStringLiteral("favorite")).toBool() ? 1 : 0);
                    if (!insertFolder.exec()) {
                        emit errorOccurred(QStringLiteral("创建文件夹失败：%1").arg(name));
                        continue;
                    }
                }
                m_mergeFolderMap[oldId] = newId;
                for (const auto& oldTagId : tagsByFolder.value(oldId)) {
                    if (m_mergeTagMap.contains(oldTagId)) {
                        // 文件夹标签迁移为尽力而为，失败不影响文件夹合并主流程
                        (void)m_repository.addItemTag(newId, m_mergeTagMap.value(oldTagId), &error);
                    }
                }
                mergeFolderLevel(oldId, newId);
            }
        };
        mergeFolderLevel(QString(), QString());
    }

    // 3. 构建当前库哈希索引（文件内容判重）
    {
        const auto currentScores = m_repository.list(QString(), &error);
        for (const auto& score : currentScores) {
            const auto hash = sha256OfFile(score.filePath);
            if (hash.isEmpty() || m_mergeHashIndex.contains(hash)) continue;
            m_mergeHashIndex[hash] = score.id;
            m_mergeScoreTitles[score.id] = score.title;
        }
    }

    // 4. 构建合并队列（备份乐谱 + 源文件路径 + 哈希）
    for (const auto& value : backupScores) {
        auto map = value.toMap();
        const auto sourcePath = m_mergeBackupRoot + QStringLiteral("/library/scores/")
            + map.value(QStringLiteral("fileName")).toString();
        if (!QFileInfo::exists(sourcePath)) continue;
        map[QStringLiteral("filePath")] = sourcePath;
        map[QStringLiteral("hash")] = sha256OfFile(sourcePath);
        QVariantList tagIds;
        const auto backupTagIds = tagsByScore.value(map.value(QStringLiteral("id")).toString());
        for (const auto& tagId : backupTagIds) tagIds.append(tagId);
        map[QStringLiteral("tagIds")] = tagIds;
        map[QStringLiteral("annotations")] = annotationsByScore.value(map.value(QStringLiteral("id")).toString());
        m_mergeQueue.append(map);
    }

    // 5. 开始合并（逐项处理，冲突时暂停等待用户决策）
    if (m_mergeQueue.isEmpty()) {
        reloadFolders();
        reload();
        emit noticeOccurred(QStringLiteral("备份中无新增乐谱，标签与文件夹已合并"));
        emit mergeFinished(0);
        cleanupMergeState();
        return {};
    }
    continueMerge();
    return {};
}

void LibraryService::importBackupScore(const QVariantMap& item, const QString& targetFolderId)
{
    const auto newId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const auto sourcePath = item.value(QStringLiteral("filePath")).toString();
    QString error;
    const auto storedPath = FileService::copyScoreIntoLibrary(sourcePath, newId, &error);
    if (storedPath.isEmpty()) {
        emit errorOccurred(error.isEmpty() ? QStringLiteral("导入乐谱失败。") : error);
        return;
    }
    const auto fileType = item.value(QStringLiteral("fileType")).toString();
    Score score;
    score.id = newId;
    score.title = item.value(QStringLiteral("title")).toString();
    score.composer = item.value(QStringLiteral("composer")).toString();
    score.fileName = QFileInfo(storedPath).fileName();
    score.filePath = storedPath;
    score.fileType = fileType;
    score.pageCount = item.value(QStringLiteral("pageCount")).toInt();
    score.favorite = item.value(QStringLiteral("favorite")).toBool();
    score.lastPage = item.value(QStringLiteral("lastPage")).toInt();
    score.createdAt = QDateTime::fromMSecsSinceEpoch(item.value(QStringLiteral("createdAt")).toLongLong());
    score.updatedAt = QDateTime::fromMSecsSinceEpoch(item.value(QStringLiteral("updatedAt")).toLongLong());
    const auto lastOpenedAt = item.value(QStringLiteral("lastOpenedAt")).toLongLong();
    if (lastOpenedAt > 0) score.lastOpenedAt = QDateTime::fromMSecsSinceEpoch(lastOpenedAt);
    if (!m_repository.insert(score, targetFolderId, &error)) {
        (void)FileService::removeFile(storedPath, &error); // 回滚：尽力删除已复制文件
        emit errorOccurred(QStringLiteral("导入乐谱失败：%1").arg(score.title));
        return;
    }
    if (FileService::isSupportedScoreFile(storedPath)) {
        m_thumbnailGenerator.generate(newId, storedPath, fileType);
    }
    const auto tagIds = item.value(QStringLiteral("tagIds")).toList();
    for (const auto& tagIdVariant : tagIds) {
        const auto tagId = tagIdVariant.toString();
        // 乐谱标签迁移为尽力而为，失败不影响导入主流程
        if (m_mergeTagMap.contains(tagId)) (void)m_repository.addTag(newId, m_mergeTagMap.value(tagId), &error);
    }
    const auto annotations = item.value(QStringLiteral("annotations")).toList();
    if (!annotations.isEmpty()) {
        QSqlQuery insertAnnotation(m_databaseService.database());
        for (const auto& value : annotations) {
            const auto annotation = value.toMap();
            insertAnnotation.prepare(QStringLiteral(
                "INSERT INTO annotations (id, score_id, page, type, data, created_at, updated_at) VALUES (?, ?, ?, ?, ?, ?, ?)"));
            insertAnnotation.addBindValue(QUuid::createUuid().toString(QUuid::WithoutBraces));
            insertAnnotation.addBindValue(newId);
            insertAnnotation.addBindValue(annotation.value(QStringLiteral("page")).toInt());
            insertAnnotation.addBindValue(annotation.value(QStringLiteral("type")).toString());
            insertAnnotation.addBindValue(annotation.value(QStringLiteral("data")).toString());
            insertAnnotation.addBindValue(annotation.value(QStringLiteral("createdAt")).toLongLong());
            insertAnnotation.addBindValue(annotation.value(QStringLiteral("updatedAt")).toLongLong());
            insertAnnotation.exec();
        }
    }
}

void LibraryService::continueMerge()
{
    QString error;
    int processed = 0;
    while (m_mergeIndex < m_mergeQueue.size()) {
        const auto item = m_mergeQueue[m_mergeIndex].toMap();
        const auto title = item.value(QStringLiteral("title")).toString();
        const auto hash = item.value(QStringLiteral("hash")).toString();
        const auto targetFolderId = m_mergeFolderMap.value(item.value(QStringLiteral("folderId")).toString());

        QString existingScoreId;
        if (!hash.isEmpty() && m_mergeHashIndex.contains(hash)) existingScoreId = m_mergeHashIndex.value(hash);

        // 当前项总是应用用户刚做出的决策；applyToAll 只决定后续项是否复用
        QString action = m_mergeConflictAction;
        if (!existingScoreId.isEmpty() && action.isEmpty()) {
            emit mergeConflict(title, m_mergeScoreTitles.value(existingScoreId), m_mergeIndex, m_mergeQueue.size());
            return;
        }

        if (!existingScoreId.isEmpty()) {
            if (action == QStringLiteral("skip")) {
                ++m_mergeIndex;
                continue;
            }
            if (action == QStringLiteral("overwrite")) {
                const auto filePath = m_repository.filePathById(existingScoreId, &error);
                const auto thumbnailPath = m_repository.thumbnailPathById(existingScoreId, &error);
                (void)FileService::removeFile(filePath, &error);
                (void)FileService::removeFile(thumbnailPath, &error);
                if (!m_repository.remove(existingScoreId, &error)) {
                    // 覆盖删除失败：跳过该项，避免同一内容产生两份记录
                    ++m_mergeIndex;
                    continue;
                }
                m_mergeHashIndex.remove(hash);
            }
        }

        importBackupScore(item, targetFolderId);
        ++processed;
        ++m_mergeIndex;
        // 非"应用到所有"时，当前项决策用完即清空，下一项冲突时重新弹窗
        if (!m_mergeApplyToAll) m_mergeConflictAction.clear();
    }

    reloadFolders();
    reloadTags();
    reload();
    emit noticeOccurred(processed > 0
        ? QStringLiteral("已合并导入 %1 份乐谱").arg(processed)
        : QStringLiteral("备份已合并，无新增乐谱"));
    emit mergeFinished(processed);
    cleanupMergeState();
}

void LibraryService::resolveMergeConflict(const QString& action, bool applyToAll)
{
    m_mergeConflictAction = action;
    m_mergeApplyToAll = applyToAll;
    if (action == QStringLiteral("cancel")) {
        cleanupMergeState();
        emit noticeOccurred(QStringLiteral("合并已取消"));
        emit mergeFinished(0);
        return;
    }
    continueMerge();
}
