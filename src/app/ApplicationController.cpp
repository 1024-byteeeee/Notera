#include "app/ApplicationController.h"
#include "platform/AppDataPaths.h"

#include <algorithm>
#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QDesktopServices>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUuid>
#include <QtCore/private/qzipreader_p.h>
#include <QtCore/private/qzipwriter_p.h>

ApplicationController::ApplicationController(QObject* parent)
    : QObject(parent)
{
    const QSettings settings;
    m_themeMode = settings.value(QStringLiteral("appearance/themeMode"), 0).toInt();
    if (m_themeMode < 0 || m_themeMode > 1) {
        m_themeMode = 0;
    }
    m_animationsEnabled = settings.value(QStringLiteral("appearance/animationsEnabled"), true).toBool();
    m_defaultScrollSpeed = settings.value(QStringLiteral("reader/defaultScrollSpeed"), 15.0).toDouble();
    if (m_defaultScrollSpeed < 1.0 || m_defaultScrollSpeed > 256.0) {
        m_defaultScrollSpeed = 15.0;
    }
    m_autoScrollSpeed = m_defaultScrollSpeed;
}

int ApplicationController::themeMode() const
{
    return m_themeMode;
}

void ApplicationController::setThemeMode(const int themeMode)
{
    if (themeMode < 0 || themeMode > 1 || m_themeMode == themeMode) {
        return;
    }

    m_themeMode = themeMode;
    QSettings().setValue(QStringLiteral("appearance/themeMode"), themeMode);
    emit themeModeChanged();
}

bool ApplicationController::animationsEnabled() const
{
    return m_animationsEnabled;
}

void ApplicationController::setAnimationsEnabled(const bool enabled)
{
    if (m_animationsEnabled == enabled) {
        return;
    }
    m_animationsEnabled = enabled;
    QSettings().setValue(QStringLiteral("appearance/animationsEnabled"), enabled);
    emit animationsEnabledChanged();
}

QString ApplicationController::currentScoreTitle() const
{
    return m_currentScoreTitle;
}

QUrl ApplicationController::currentFileUrl() const
{
    return m_currentFileUrl;
}

QString ApplicationController::currentFileType() const
{
    return m_currentFileType;
}

QString ApplicationController::currentScoreId() const
{
    return m_currentScoreId;
}

QString ApplicationController::currentScoreFolderId() const
{
    return m_currentScoreFolderId;
}

int ApplicationController::currentScorePageCount() const
{
    return m_currentScorePageCount;
}

double ApplicationController::autoScrollSpeed() const
{
    return m_autoScrollSpeed;
}

void ApplicationController::setAutoScrollSpeed(const double speed)
{
    const auto boundedSpeed = std::clamp(speed, 1.0, 256.0);
    if (qFuzzyCompare(m_autoScrollSpeed, boundedSpeed)) {
        return;
    }
    m_autoScrollSpeed = boundedSpeed;
    emit autoScrollSpeedChanged();
}

double ApplicationController::defaultScrollSpeed() const
{
    return m_defaultScrollSpeed;
}

void ApplicationController::setDefaultScrollSpeed(const double speed)
{
    const auto boundedSpeed = std::clamp(speed, 1.0, 256.0);
    if (qFuzzyCompare(m_defaultScrollSpeed, boundedSpeed)) {
        return;
    }
    m_defaultScrollSpeed = boundedSpeed;
    QSettings().setValue(QStringLiteral("reader/defaultScrollSpeed"), boundedSpeed);
    emit defaultScrollSpeedChanged();
}

void ApplicationController::openScore(const QString& scoreId, const QString& title, const QString& filePath,
    const QString& fileType, const int pageCount, const QString& folderId)
{
    m_currentScoreId = scoreId;
    m_currentScoreTitle = title;
    m_currentFileUrl = QUrl::fromLocalFile(filePath);
    m_currentFileType = fileType.toLower();
    m_currentScoreFolderId = folderId;
    m_currentScorePageCount = pageCount;
    m_autoScrollSpeed = m_defaultScrollSpeed;
    emit currentScoreChanged();
    emit autoScrollSpeedChanged();
    emit scoreOpened(scoreId);
    setCurrentPage(QStringLiteral("reader"));
}

QString ApplicationController::currentPage() const
{
    return m_currentPage;
}

void ApplicationController::setCurrentPage(const QString& page)
{
    if (m_currentPage == page) {
        return;
    }

    m_currentPage = page;
    emit currentPageChanged();
}

QString ApplicationController::libraryFilter() const
{
    return m_libraryFilter;
}

void ApplicationController::setLibraryFilter(const QString& filter)
{
    if (m_libraryFilter == filter) {
        return;
    }
    m_libraryFilter = filter;
    emit libraryFilterChanged();
}

QString ApplicationController::dataDirectory() const
{
    return AppDataPaths::root();
}

QString ApplicationController::pendingDataDirectory() const
{
    return QSettings().value(QStringLiteral("storage/pendingDataDirectory")).toString();
}

static bool copyDirectoryRecursively(const QString& src, const QString& dst, QString* error)
{
    QDir srcDir(src);
    if (!srcDir.exists()) {
        return true;
    }
    QDir().mkpath(dst);
    const auto entries = srcDir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries | QDir::Hidden);
    for (const auto& entry : entries) {
        const auto dstPath = dst + QLatin1Char('/') + entry.fileName();
        if (entry.isDir()) {
            if (!copyDirectoryRecursively(entry.absoluteFilePath(), dstPath, error)) {
                return false;
            }
        } else {
            if (!QFile::copy(entry.absoluteFilePath(), dstPath)) {
                if (error) {
                    *error = QObject::tr("复制文件失败: %1").arg(entry.absoluteFilePath());
                }
                return false;
            }
        }
    }
    return true;
}

static bool removeDirectoryRecursively(const QString& path)
{
    QDir dir(path);
    if (!dir.exists()) {
        return true;
    }
    return dir.removeRecursively();
}

static bool zipDirectory(const QString& srcDir, const QString& zipPath, QString* error)
{
    QZipWriter writer(zipPath);
    writer.setCompressionPolicy(QZipWriter::AutoCompress);
    if (writer.status() != QZipWriter::NoError) {
        if (error) *error = QStringLiteral("无法创建备份压缩包。");
        return false;
    }

    QDirIterator it(srcDir, QDir::NoDotAndDotDot | QDir::AllEntries | QDir::Hidden,
        QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const auto filePath = it.next();
        const auto relativePath = QDir(srcDir).relativeFilePath(filePath);
        const QFileInfo info(filePath);
        if (info.isDir()) {
            writer.addDirectory(relativePath);
        } else {
            QFile file(filePath);
            if (!file.open(QIODevice::ReadOnly)) {
                if (error) *error = QStringLiteral("无法读取文件：%1").arg(filePath);
                return false;
            }
            writer.addFile(relativePath, file.readAll());
            file.close();
        }
    }
    writer.close();
    if (writer.status() != QZipWriter::NoError) {
        if (error) *error = QStringLiteral("写入备份压缩包失败。");
        return false;
    }
    return true;
}

static bool unzipToDirectory(const QString& zipPath, const QString& dstDir, QString* error)
{
    QZipReader reader(zipPath);
    if (!reader.exists()) {
        if (error) *error = QStringLiteral("无法打开备份压缩包。");
        return false;
    }
    const auto entries = reader.fileInfoList();
    for (const auto& entry : entries) {
        const auto targetPath = QDir(dstDir).filePath(entry.filePath);
        if (entry.isDir) {
            if (!QDir().mkpath(targetPath)) {
                if (error) *error = QStringLiteral("无法创建目录：%1").arg(targetPath);
                return false;
            }
        } else {
            if (!QDir().mkpath(QFileInfo(targetPath).absolutePath())) {
                if (error) *error = QStringLiteral("无法创建目录：%1").arg(QFileInfo(targetPath).absolutePath());
                return false;
            }
            QFile file(targetPath);
            if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                if (error) *error = QStringLiteral("无法写入文件：%1").arg(targetPath);
                return false;
            }
            file.write(reader.fileData(entry.filePath));
            file.close();
        }
    }
    reader.close();
    return true;
}

static QString escapedSqlString(QString value)
{
    return value.replace(QLatin1Char('\''), QStringLiteral("''"));
}

static bool writeBackupManifest(const QString& backupRoot, const QString& sourceRoot, QString* error)
{
    QFile file(backupRoot + QStringLiteral("/manifest.json"));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        *error = QStringLiteral("无法写入备份清单。");
        return false;
    }
    const QJsonObject manifest {
        {QStringLiteral("format"), QStringLiteral("notera-backup")},
        {QStringLiteral("formatVersion"), 1},
        {QStringLiteral("applicationVersion"), QCoreApplication::applicationVersion()},
        {QStringLiteral("createdAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
        {QStringLiteral("sourceRoot"), sourceRoot}
    };
    if (file.write(QJsonDocument(manifest).toJson(QJsonDocument::Indented)) < 0) {
        *error = QStringLiteral("无法写入备份清单。");
        return false;
    }
    return true;
}

static bool readBackupManifest(const QString& backupRoot, QJsonObject* manifest, QString* error)
{
    QFile file(backupRoot + QStringLiteral("/manifest.json"));
    if (!file.open(QIODevice::ReadOnly)) {
        *error = QStringLiteral("所选目录不是 Notera 数据库备份。");
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

static bool validateBackupDatabase(const QString& databasePath, QString* error)
{
    const auto connectionName = QStringLiteral("notera_backup_validation_")
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

QString ApplicationController::migrateDataDirectory(const QUrl& newDirectory)
{
    if (!newDirectory.isValid() || !newDirectory.isLocalFile()) {
        return QStringLiteral("请选择本机文件夹。");
    }
    const auto newPath = newDirectory.toLocalFile();
    const auto oldPath = AppDataPaths::root();
    const auto cleanNewPath = QDir::cleanPath(newPath);

    if (cleanNewPath.isEmpty() || cleanNewPath == oldPath) {
        return QStringLiteral("路径无效或与当前路径相同。");
    }
    const auto oldPrefix = QDir::cleanPath(oldPath) + QDir::separator();
    if (cleanNewPath.startsWith(oldPrefix, Qt::CaseInsensitive)) {
        return QStringLiteral("新数据目录不能位于当前数据目录内部。");
    }

    QDir newDir(cleanNewPath);
    if (newDir.exists() && !newDir.isEmpty()) {
        return QStringLiteral("目标目录不为空，请选择一个空目录或新建目录。");
    }

    if (!QDir().mkpath(cleanNewPath)) {
        return QStringLiteral("无法创建目标目录。");
    }

    QSettings settings;
    settings.setValue(QStringLiteral("storage/pendingDataDirectory"), cleanNewPath);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        return QStringLiteral("无法保存新的数据目录设置。");
    }
    emit dataDirectoryChanged();
    return {};
}

QString ApplicationController::openDataDirectory() const
{
    const auto path = AppDataPaths::root();
    if (!QDir(path).exists()) {
        return QStringLiteral("数据存储目录不存在。");
    }
    return QDesktopServices::openUrl(QUrl::fromLocalFile(path))
        ? QString {} : QStringLiteral("无法打开数据存储目录。");
}

QString ApplicationController::exportDatabaseBackup(const QUrl& destinationFile) const
{
    if (!destinationFile.isValid() || !destinationFile.isLocalFile()) {
        return QStringLiteral("请选择本机保存位置。");
    }
    auto zipPath = QDir::cleanPath(destinationFile.toLocalFile());
    if (!zipPath.endsWith(QStringLiteral(".zip"), Qt::CaseInsensitive)
        && !zipPath.endsWith(QStringLiteral(".notera-backup"), Qt::CaseInsensitive)) {
        zipPath += QStringLiteral(".notera-backup.zip");
    }
    const auto parentPath = QFileInfo(zipPath).absolutePath();
    if (!QDir(parentPath).exists()) return QStringLiteral("目标文件夹不存在。");

    QTemporaryDir tempDir;
    if (!tempDir.isValid()) return QStringLiteral("无法创建临时目录。");
    const auto backupRoot = tempDir.path();
    if (!QDir().mkpath(backupRoot + QStringLiteral("/database"))) {
        return QStringLiteral("无法创建备份目录。");
    }

    QString error;
    const auto snapshotPath = backupRoot + QStringLiteral("/database/notera.db");
    auto database = QSqlDatabase::database(QStringLiteral("notera-library"), false);
    QSqlQuery query(database);
    const auto snapshotSql = QStringLiteral("VACUUM INTO '%1'").arg(escapedSqlString(snapshotPath));
    if (!database.isOpen() || !query.exec(snapshotSql)) {
        return QStringLiteral("创建数据库一致性快照失败：%1").arg(query.lastError().text());
    }

    const auto sourceRoot = AppDataPaths::root();
    const QStringList directories {QStringLiteral("library"), QStringLiteral("thumbnails"),
        QStringLiteral("annotations")};
    for (const auto& directory : directories) {
        if (!copyDirectoryRecursively(sourceRoot + QLatin1Char('/') + directory,
                backupRoot + QLatin1Char('/') + directory, &error)) {
            return error;
        }
    }
    if (!writeBackupManifest(backupRoot, sourceRoot, &error)) {
        return error;
    }

    if (QFileInfo::exists(zipPath) && !QFile::remove(zipPath)) {
        return QStringLiteral("无法覆盖已存在的备份文件。");
    }
    if (!zipDirectory(backupRoot, zipPath, &error)) {
        QFile::remove(zipPath);
        return error;
    }
    return {};
}

QString ApplicationController::importDatabaseBackup(const QUrl& backupFile)
{
    if (!backupFile.isValid() || !backupFile.isLocalFile()) {
        return QStringLiteral("请选择本机备份文件。");
    }
    const auto zipPath = QDir::cleanPath(backupFile.toLocalFile());
    if (!QFileInfo::exists(zipPath)) return QStringLiteral("备份文件不存在。");

    QTemporaryDir tempDir;
    if (!tempDir.isValid()) return QStringLiteral("无法创建临时目录。");
    const auto backupRoot = tempDir.path();

    QString error;
    if (!unzipToDirectory(zipPath, backupRoot, &error)) {
        return error;
    }

    QJsonObject manifest;
    if (!readBackupManifest(backupRoot, &manifest, &error)
        || !validateBackupDatabase(backupRoot + QStringLiteral("/database/notera.db"), &error)) {
        return error;
    }

    const auto currentRoot = QDir::cleanPath(AppDataPaths::root());
    const auto parent = QFileInfo(currentRoot).absolutePath();
    const auto stagedRoot = QDir(parent).filePath(QStringLiteral(".notera-restore-")
        + QUuid::createUuid().toString(QUuid::WithoutBraces));
    if (!copyDirectoryRecursively(backupRoot, stagedRoot, &error)) {
        removeDirectoryRecursively(stagedRoot);
        return error;
    }
    QSettings settings;
    settings.setValue(QStringLiteral("storage/pendingBackupRestore"), stagedRoot);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        removeDirectoryRecursively(stagedRoot);
        return QStringLiteral("无法保存数据库导入任务。");
    }
    emit restartRequested();
    return {};
}

bool ApplicationController::applyPendingBackupRestore(QString* error)
{
    QSettings settings;
    const auto stagedRoot = QDir::cleanPath(
        settings.value(QStringLiteral("storage/pendingBackupRestore")).toString());
    if (stagedRoot.isEmpty()) return true;
    QJsonObject manifest;
    if (!readBackupManifest(stagedRoot, &manifest, error)
        || !validateBackupDatabase(stagedRoot + QStringLiteral("/database/notera.db"), error)) {
        return false;
    }

    const auto currentRoot = QDir::cleanPath(AppDataPaths::root());
    const auto rollbackRoot = QDir(QFileInfo(currentRoot).absolutePath()).filePath(
        QStringLiteral(".notera-rollback-") + QUuid::createUuid().toString(QUuid::WithoutBraces));
    if (QDir(currentRoot).exists() && !QDir().rename(currentRoot, rollbackRoot)) {
        *error = QStringLiteral("无法暂存当前数据，数据库导入已取消。");
        return false;
    }
    if (!QDir().rename(stagedRoot, currentRoot)) {
        if (QDir(rollbackRoot).exists()) QDir().rename(rollbackRoot, currentRoot);
        *error = QStringLiteral("无法启用导入的数据，原数据已保留。");
        return false;
    }

    const auto databasePath = currentRoot + QStringLiteral("/database/notera.db");
    const auto connectionName = QStringLiteral("notera_backup_path_update");
    bool updated = false;
    {
        auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(databasePath);
        if (database.open() && database.transaction()) {
            const auto sourceRoot = manifest.value(QStringLiteral("sourceRoot")).toString();
            QSqlQuery query(database);
            query.prepare(QStringLiteral("UPDATE scores SET file_path = REPLACE(file_path, ?, ?)"));
            query.addBindValue(sourceRoot);
            query.addBindValue(currentRoot);
            const auto filesUpdated = query.exec();
            query.prepare(QStringLiteral("UPDATE scores SET thumbnail_path = REPLACE(thumbnail_path, ?, ?) WHERE thumbnail_path IS NOT NULL"));
            query.addBindValue(sourceRoot);
            query.addBindValue(currentRoot);
            updated = filesUpdated && query.exec() && database.commit();
            if (!updated) database.rollback();
        }
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
    if (!updated) {
        removeDirectoryRecursively(currentRoot);
        QDir().rename(rollbackRoot, currentRoot);
        *error = QStringLiteral("无法更新导入数据库中的资源路径，原数据已恢复。");
        return false;
    }
    removeDirectoryRecursively(rollbackRoot);
    settings.remove(QStringLiteral("storage/pendingBackupRestore"));
    settings.sync();
    return settings.status() == QSettings::NoError;
}

void ApplicationController::requestRestart()
{
    emit restartRequested();
}

QString ApplicationController::clearAllData(const QString& confirmation)
{
    if (confirmation != QStringLiteral("确认清空所有数据")) {
        return QStringLiteral("请输入完整的“确认清空所有数据”。");
    }
    const auto clearRoot = QDir::cleanPath(AppDataPaths::root());
    if (clearRoot.isEmpty() || clearRoot == QDir::rootPath()
        || clearRoot == QDir::homePath()) {
        return QStringLiteral("数据目录不安全，已取消清空操作。");
    }
    if (!QFileInfo::exists(clearRoot + QStringLiteral("/database/notera.db"))) {
        return QStringLiteral("未找到 Notera 数据库，已取消清空操作。");
    }
    QSettings settings;
    settings.clear();
    settings.setValue(QStringLiteral("storage/pendingClearRoot"), clearRoot);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        return QStringLiteral("无法保存清空任务，请稍后重试。");
    }
    emit restartRequested();
    return {};
}

bool ApplicationController::applyPendingDataClear(QString* error)
{
    QSettings settings;
    const auto clearRoot = QDir::cleanPath(
        settings.value(QStringLiteral("storage/pendingClearRoot")).toString());
    if (clearRoot.isEmpty()) return true;
    if (clearRoot == QDir::rootPath() || clearRoot == QDir::homePath()) {
        *error = QStringLiteral("拒绝清空不安全的数据目录。");
        return false;
    }
    if (!QFileInfo::exists(clearRoot + QStringLiteral("/database/notera.db"))) {
        *error = QStringLiteral("待清空目录不是有效的 Notera 数据目录。");
        return false;
    }
    if (!removeDirectoryRecursively(clearRoot)) {
        *error = QStringLiteral("无法清空旧数据目录。");
        return false;
    }
    settings.remove(QStringLiteral("storage/pendingClearRoot"));
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        *error = QStringLiteral("无法完成清空状态更新。");
        return false;
    }
    return true;
}

bool ApplicationController::applyPendingDataMigration(QString* error)
{
    QSettings settings;
    const auto cleanNewPath = QDir::cleanPath(
        settings.value(QStringLiteral("storage/pendingDataDirectory")).toString());
    if (cleanNewPath.isEmpty()) return true;

    const auto oldPath = AppDataPaths::root();
    if (cleanNewPath == oldPath) {
        settings.remove(QStringLiteral("storage/pendingDataDirectory"));
        return true;
    }
    QDir newDir(cleanNewPath);
    if (newDir.exists() && !newDir.isEmpty()) {
        *error = QStringLiteral("待迁移的目标目录不再为空。");
        return false;
    }
    if (!QDir().mkpath(cleanNewPath)) {
        *error = QStringLiteral("无法创建目标目录。");
        return false;
    }

    QString copyError;
    if (!copyDirectoryRecursively(oldPath, cleanNewPath, &copyError)) {
        removeDirectoryRecursively(cleanNewPath);
        *error = copyError.isEmpty() ? QStringLiteral("复制文件失败。") : copyError;
        return false;
    }

    const auto dbPath = cleanNewPath + QLatin1String("/database/notera.db");
    if (QFileInfo::exists(dbPath)) {
        const auto connName = QStringLiteral("notera_pending_migration");
        bool databaseUpdated = false;
        {
            auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connName);
            db.setDatabaseName(dbPath);
            if (db.open() && db.transaction()) {
                QSqlQuery query(db);
                query.prepare(QStringLiteral("UPDATE scores SET file_path = REPLACE(file_path, ?, ?)"));
                query.addBindValue(oldPath);
                query.addBindValue(cleanNewPath);
                const auto filesUpdated = query.exec();
                query.prepare(QStringLiteral(
                    "UPDATE scores SET thumbnail_path = REPLACE(thumbnail_path, ?, ?) WHERE thumbnail_path IS NOT NULL"));
                query.addBindValue(oldPath);
                query.addBindValue(cleanNewPath);
                const auto thumbnailsUpdated = query.exec();
                databaseUpdated = filesUpdated && thumbnailsUpdated && db.commit();
                if (!databaseUpdated) db.rollback();
            }
            db.close();
        }
        QSqlDatabase::removeDatabase(connName);
        if (!databaseUpdated) {
            removeDirectoryRecursively(cleanNewPath);
            *error = QStringLiteral("无法更新迁移后数据库中的文件路径。");
            return false;
        }
    }

    settings.setValue(QStringLiteral("storage/dataDirectory"), cleanNewPath);
    settings.remove(QStringLiteral("storage/pendingDataDirectory"));
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        removeDirectoryRecursively(cleanNewPath);
        *error = QStringLiteral("无法保存迁移后的数据目录设置。");
        return false;
    }
    AppDataPaths::setCustomRoot(cleanNewPath);
    removeDirectoryRecursively(oldPath);
    return true;
}
