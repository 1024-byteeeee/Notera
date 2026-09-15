#pragma once

#include <QJsonObject>
#include <QString>

namespace BackupArchiveService
{

// Extracts a user-supplied backup archive only after validating every archive entry.
[[nodiscard]] bool extractToDirectory(const QString& archivePath,
                                      const QString& destinationDirectory, QString* error);

[[nodiscard]] bool readManifest(const QString& backupDirectory, QJsonObject* manifest,
                                QString* error);

[[nodiscard]] bool validateDatabase(const QString& databasePath, QString* error);

} // namespace BackupArchiveService
