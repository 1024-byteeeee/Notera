#include "services/BackupArchiveService.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QUuid>
#include <QtCore/private/qzipreader_p.h>

namespace
{

constexpr qsizetype MaxArchiveEntries = 10'000;
constexpr qint64 MaxArchiveBytes = 1024LL * 1024 * 1024;

bool safeRelativePath(const QString& archivePath, QString* relativePath)
{
    QString normalized = archivePath;
    normalized.replace(QLatin1Char('\\'), QLatin1Char('/'));
    if (normalized.isEmpty() || QDir::isAbsolutePath(normalized) ||
        (normalized.size() >= 2 && normalized.at(1) == QLatin1Char(':')))
    {
        return false;
    }

    normalized = QDir::cleanPath(normalized);
    if (normalized == QStringLiteral(".") || normalized == QStringLiteral("..") ||
        normalized.startsWith(QStringLiteral("../")))
    {
        return false;
    }
    *relativePath = normalized;
    return true;
}

bool hasRequiredTable(const QSqlDatabase& database, const QString& tableName)
{
    QSqlQuery query(database);
    query.prepare(QStringLiteral("SELECT 1 FROM sqlite_master WHERE type = 'table' AND name = ?"));
    query.addBindValue(tableName);
    return query.exec() && query.next();
}

} // namespace

namespace BackupArchiveService
{

bool extractToDirectory(const QString& archivePath, const QString& destinationDirectory,
                        QString* error)
{
    QZipReader reader(archivePath);
    if (!reader.exists() || !reader.isReadable())
    {
        *error = QStringLiteral("无法打开备份压缩包");
        return false;
    }

    const QString destinationRoot =
        QDir::cleanPath(QFileInfo(destinationDirectory).absoluteFilePath());
    if (destinationRoot.isEmpty() || !QDir().mkpath(destinationRoot))
    {
        *error = QStringLiteral("无法创建备份临时目录");
        return false;
    }

    const auto entries = reader.fileInfoList();
    if (entries.size() > MaxArchiveEntries)
    {
        *error = QStringLiteral("备份包含过多文件");
        return false;
    }

    QSet<QString> extractedPaths;
    qint64 totalBytes = 0;
    for (const auto& entry : entries)
    {
        QString relativePath;
        if (!entry.isValid() || entry.isSymLink ||
            !safeRelativePath(entry.filePath, &relativePath) ||
            extractedPaths.contains(relativePath))
        {
            *error = QStringLiteral("备份包含不安全的文件路径");
            return false;
        }
        extractedPaths.insert(relativePath);

        if (!entry.isDir && !entry.isFile)
        {
            *error = QStringLiteral("备份包含不支持的条目");
            return false;
        }
        if (entry.size < 0 || (entry.isFile && (entry.size > MaxArchiveBytes ||
                                                totalBytes > MaxArchiveBytes - entry.size)))
        {
            *error = QStringLiteral("备份解压后的大小超出限制");
            return false;
        }
        if (entry.isFile)
            totalBytes += entry.size;

        const QString targetPath = QDir::cleanPath(QDir(destinationRoot).filePath(relativePath));
        if (targetPath != destinationRoot &&
            !targetPath.startsWith(destinationRoot + QLatin1Char('/')))
        {
            *error = QStringLiteral("备份包含不安全的文件路径");
            return false;
        }

        if (entry.isDir)
        {
            if (!QDir().mkpath(targetPath))
            {
                *error = QStringLiteral("无法创建目录：%1").arg(relativePath);
                return false;
            }
            continue;
        }

        if (!QDir().mkpath(QFileInfo(targetPath).absolutePath()))
        {
            *error = QStringLiteral("无法创建目录：%1").arg(relativePath);
            return false;
        }
        const QByteArray data = reader.fileData(entry.filePath);
        if (reader.status() != QZipReader::NoError || data.size() != entry.size)
        {
            *error = QStringLiteral("无法读取备份文件：%1").arg(relativePath);
            return false;
        }
        QFile file(targetPath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate) ||
            file.write(data) != data.size())
        {
            *error = QStringLiteral("无法写入备份文件：%1").arg(relativePath);
            return false;
        }
    }
    reader.close();
    return true;
}

bool readManifest(const QString& backupDirectory, QJsonObject* manifest, QString* error)
{
    QFile file(QDir(backupDirectory).filePath(QStringLiteral("manifest.json")));
    if (!file.open(QIODevice::ReadOnly))
    {
        *error = QStringLiteral("所选文件不是 Notera 数据库备份");
        return false;
    }
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        *error = QStringLiteral("备份清单已损坏");
        return false;
    }
    *manifest = document.object();
    if (manifest->value(QStringLiteral("format")).toString() != QStringLiteral("notera-backup") ||
        manifest->value(QStringLiteral("formatVersion")).toInt() != 1 ||
        !QFileInfo::exists(QDir(backupDirectory).filePath(QStringLiteral("database/notera.db"))))
    {
        *error = QStringLiteral("备份格式不受支持或数据库文件缺失");
        return false;
    }
    return true;
}

bool validateDatabase(const QString& databasePath, QString* error)
{
    const QString connectionName = QStringLiteral("notera_backup_validation_") +
                                   QUuid::createUuid().toString(QUuid::WithoutBraces);
    bool valid = false;
    {
        auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        database.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
        database.setDatabaseName(databasePath);
        if (database.open())
        {
            QSqlQuery integrity(database);
            const bool integrityOk = integrity.exec(QStringLiteral("PRAGMA integrity_check")) &&
                                     integrity.next() &&
                                     integrity.value(0).toString() == QStringLiteral("ok");
            QSqlQuery foreignKeys(database);
            const bool foreignKeysOk =
                foreignKeys.exec(QStringLiteral("PRAGMA foreign_key_check")) && !foreignKeys.next();
            const QStringList requiredTables{
                QStringLiteral("scores"),      QStringLiteral("folders"),
                QStringLiteral("tags"),        QStringLiteral("score_tags"),
                QStringLiteral("folder_tags"), QStringLiteral("annotations")};
            valid = integrityOk && foreignKeysOk;
            for (const auto& table : requiredTables)
                valid = valid && hasRequiredTable(database, table);
        }
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
    if (!valid)
        *error = QStringLiteral("备份数据库完整性或结构校验失败");
    return valid;
}

} // namespace BackupArchiveService
