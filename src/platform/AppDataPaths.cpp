#include "platform/AppDataPaths.h"

#include <QDir>
#include <QSettings>
#include <QStandardPaths>

namespace {

QString g_customRoot;

QString childDirectory(const QString& name)
{
    QDir directory(AppDataPaths::root());
    directory.mkpath(name);
    return directory.filePath(name);
}

} // namespace

namespace AppDataPaths {

QString defaultRoot()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
}

QString root()
{
    QString path = g_customRoot.isEmpty()
        ? QSettings().value(QStringLiteral("storage/dataDirectory"), QString()).toString()
        : g_customRoot;
    if (path.isEmpty()) {
        path = defaultRoot();
    }
    QDir().mkpath(path);
    return path;
}

void setCustomRoot(const QString& path)
{
    g_customRoot = path;
}

QString databaseDirectory()
{
    return childDirectory(QStringLiteral("database"));
}

QString libraryDirectory()
{
    return childDirectory(QStringLiteral("library/scores"));
}

QString thumbnailDirectory()
{
    return childDirectory(QStringLiteral("thumbnails"));
}

QString annotationsDirectory()
{
    return childDirectory(QStringLiteral("annotations"));
}

QString cacheDirectory()
{
    return childDirectory(QStringLiteral("cache"));
}

} // namespace AppDataPaths
