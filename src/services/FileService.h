#pragma once

#include <QString>

namespace FileService {

[[nodiscard]] bool isSupportedScoreFile(const QString& sourcePath);
[[nodiscard]] bool isPreviewableImage(const QString& sourcePath);
[[nodiscard]] QString canonicalSuffix(const QString& sourcePath);
[[nodiscard]] QString copyScoreIntoLibrary(const QString& sourcePath, const QString& scoreId, QString* error);
[[nodiscard]] bool removeFile(const QString& path, QString* error);

} // namespace FileService
