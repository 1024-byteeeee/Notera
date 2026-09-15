#include "services/LibraryFileLifecycle.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUuid>

#include "platform/AppDataPaths.h"

LibraryFileLifecycle::LibraryFileLifecycle()
    : m_stagingDirectory(QDir(AppDataPaths::cacheDirectory())
                             .filePath(QStringLiteral("pending-removal-") +
                                       QUuid::createUuid().toString(QUuid::WithoutBraces)))
{
}

LibraryFileLifecycle::~LibraryFileLifecycle()
{
    if (!m_finalized)
    {
        QString ignoredError;
        (void)restore(&ignoredError);
    }
}

bool LibraryFileLifecycle::stageForRemoval(const QString& path, QString* error)
{
    if (path.isEmpty() || !QFileInfo::exists(path))
        return true;
    if (!QFileInfo(path).isFile())
    {
        *error = QStringLiteral("无法暂存非文件资源");
        return false;
    }
    if (!QDir().mkpath(m_stagingDirectory))
    {
        *error = QStringLiteral("无法创建文件回退目录");
        return false;
    }

    const QString stagedPath = QDir(m_stagingDirectory)
                                   .filePath(QString::number(m_stagedFiles.size()) +
                                             QLatin1Char('-') + QFileInfo(path).fileName());
    if (!QFile::rename(path, stagedPath))
    {
        *error = QStringLiteral("无法暂存乐谱库文件");
        return false;
    }
    m_stagedFiles.append({path, stagedPath});
    return true;
}

bool LibraryFileLifecycle::commit(QString*)
{
    if (QDir(m_stagingDirectory).exists() && !QDir(m_stagingDirectory).removeRecursively())
    {
        qWarning() << "[LibraryFileLifecycle] Deferred cleanup failed:" << m_stagingDirectory;
    }
    m_finalized = true;
    return true;
}

bool LibraryFileLifecycle::restore(QString* error)
{
    bool restored = true;
    for (auto it = m_stagedFiles.crbegin(); it != m_stagedFiles.crend(); ++it)
    {
        if (!QFileInfo::exists(it->stagedPath))
            continue;
        if (!QDir().mkpath(QFileInfo(it->originalPath).absolutePath()) ||
            !QFile::rename(it->stagedPath, it->originalPath))
        {
            restored = false;
        }
    }
    if (!restored)
    {
        *error = QStringLiteral("恢复乐谱库文件失败");
        return false;
    }
    QDir(m_stagingDirectory).removeRecursively();
    m_finalized = true;
    return true;
}
