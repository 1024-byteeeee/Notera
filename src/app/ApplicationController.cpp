#include "app/ApplicationController.h"
#include "platform/AppDataPaths.h"

#include <algorithm>
#include <QDir>
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
    m_defaultScrollSpeed = settings.value(QStringLiteral("reader/defaultScrollSpeed"), 45.0).toDouble();
    if (m_defaultScrollSpeed < 15.0 || m_defaultScrollSpeed > 160.0) {
        m_defaultScrollSpeed = 45.0;
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
    const auto boundedSpeed = std::clamp(speed, 15.0, 160.0);
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
    const auto boundedSpeed = std::clamp(speed, 15.0, 160.0);
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

bool ApplicationController::migrateDataDirectory(const QString& newPath, QString* error)
{
    const auto oldPath = AppDataPaths::root();
    const auto cleanNewPath = QDir::cleanPath(newPath);

    if (cleanNewPath.isEmpty() || cleanNewPath == oldPath) {
        if (error) {
            *error = QObject::tr("路径无效或与当前路径相同。");
        }
        return false;
    }

    QDir newDir(cleanNewPath);
    if (newDir.exists() && !newDir.isEmpty()) {
        if (error) {
            *error = QObject::tr("目标目录不为空，请选择一个空目录或新建目录。");
        }
        return false;
    }

    if (!newDir.mkpath(cleanNewPath)) {
        if (error) {
            *error = QObject::tr("无法创建目标目录。");
        }
        return false;
    }

    // 1. 复制所有数据到新目录
    if (!copyDirectoryRecursively(oldPath, cleanNewPath, error)) {
        removeDirectoryRecursively(cleanNewPath);
        return false;
    }

    // 2. 在新数据库中更新文件路径
    const auto dbPath = cleanNewPath + QLatin1String("/database/notera.db");
    if (QFileInfo::exists(dbPath)) {
        const auto connName = QStringLiteral("migration_db_") + QString::number(reinterpret_cast<quintptr>(this), 16);
        {
            auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connName);
            db.setDatabaseName(dbPath);
            if (db.open()) {
                QSqlQuery query(db);
                query.prepare(QStringLiteral("UPDATE scores SET file_path = REPLACE(file_path, ?, ?)"));
                query.addBindValue(oldPath);
                query.addBindValue(cleanNewPath);
                query.exec();
                query.prepare(QStringLiteral("UPDATE scores SET thumbnail_path = REPLACE(thumbnail_path, ?, ?)"));
                query.addBindValue(oldPath);
                query.addBindValue(cleanNewPath);
                query.exec();
                db.close();
            }
        }
        QSqlDatabase::removeDatabase(connName);
    }

    // 3. 更新设置
    QSettings().setValue(QStringLiteral("storage/dataDirectory"), cleanNewPath);
    AppDataPaths::setCustomRoot(cleanNewPath);

    // 4. 删除旧数据
    removeDirectoryRecursively(oldPath);

    emit dataDirectoryChanged();
    return true;
}
