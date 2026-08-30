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
        *error = QStringLiteral("所选乐谱文件已不存在。");
        return {};
    }

    const auto destination = AppDataPaths::libraryDirectory() + QLatin1Char('/') + scoreId + QLatin1Char('.')
        + canonicalSuffix(sourcePath);
    if (!QFile::copy(sourcePath, destination)) {
        *error = QStringLiteral("Notera 无法将乐谱复制到乐谱库。");
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
    *error = QStringLiteral("Notera 无法删除乐谱库文件。");
    return false;
}

} // namespace FileService
