#include "platform/AppDataPaths.h"

#include <QDir>
#include <QStandardPaths>

namespace {

QString childDirectory(const QString& name)
{
    QDir directory(AppDataPaths::root());
    directory.mkpath(name);
    return directory.filePath(name);
}

} // namespace

namespace AppDataPaths {

QString root()
{
    const auto path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(path);
    return path;
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
