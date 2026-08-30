#include "services/FileService.h"

#include <algorithm>
#include <QFile>
#include <QFileInfo>

#include "platform/AppDataPaths.h"

namespace {

constexpr auto SupportedSuffixes = {"pdf", "jpg", "jpeg", "png"};

} // namespace

namespace FileService {

QString canonicalSuffix(const QString& sourcePath)
{
    return QFileInfo(sourcePath).suffix().toLower();
}

bool isSupportedScoreFile(const QString& sourcePath)
{
    const auto suffix = canonicalSuffix(sourcePath);
    return std::any_of(SupportedSuffixes.begin(), SupportedSuffixes.end(), [&](const auto* item) {
        return suffix == QLatin1String(item);
    });
}

QString copyScoreIntoLibrary(const QString& sourcePath, const QString& scoreId, QString* error)
{
    const QFileInfo source(sourcePath);
    if (!source.exists() || !source.isFile()) {
        *error = QStringLiteral("The selected score file no longer exists.");
        return {};
    }

    const auto destination = AppDataPaths::libraryDirectory() + QLatin1Char('/') + scoreId + QLatin1Char('.')
        + canonicalSuffix(sourcePath);
    if (!QFile::copy(sourcePath, destination)) {
        *error = QStringLiteral("Notera could not copy the score into its library.");
        return {};
    }
    return destination;
}

bool removeFile(const QString& path, QString* error)
{
    if (path.isEmpty() || !QFileInfo::exists(path)) {
        return true;
    }
    if (QFile::remove(path)) {
        return true;
    }
    *error = QStringLiteral("Notera could not remove a library file.");
    return false;
}

} // namespace FileService
