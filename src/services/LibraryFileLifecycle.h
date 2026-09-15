#pragma once

#include <QList>
#include <QString>

class LibraryFileLifecycle final
{
  public:
    LibraryFileLifecycle();
    ~LibraryFileLifecycle();

    LibraryFileLifecycle(const LibraryFileLifecycle&) = delete;
    LibraryFileLifecycle& operator=(const LibraryFileLifecycle&) = delete;

    [[nodiscard]] bool stageForRemoval(const QString& path, QString* error);
    [[nodiscard]] bool commit(QString* error);
    [[nodiscard]] bool restore(QString* error);

  private:
    struct StagedFile
    {
        QString originalPath;
        QString stagedPath;
    };

    QString m_stagingDirectory;
    QList<StagedFile> m_stagedFiles;
    bool m_finalized{false};
};
