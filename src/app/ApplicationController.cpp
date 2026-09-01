#include "app/ApplicationController.h"
#include "platform/AppDataPaths.h"

#include <algorithm>
#include <QDir>
#include <QDesktopServices>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStandardPaths>

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

void ApplicationController::requestRestart()
{
    emit restartRequested();
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
