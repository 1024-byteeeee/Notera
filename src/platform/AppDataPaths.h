#pragma once

#include <QString>

namespace AppDataPaths {

[[nodiscard]] QString root();
[[nodiscard]] QString defaultRoot();
void setCustomRoot(const QString& path);
[[nodiscard]] QString databaseDirectory();
[[nodiscard]] QString libraryDirectory();
[[nodiscard]] QString thumbnailDirectory();
[[nodiscard]] QString annotationsDirectory();
[[nodiscard]] QString cacheDirectory();

} // namespace AppDataPaths
