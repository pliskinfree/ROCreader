#pragma once

#include "filesystem_compat.h"
#include <string>

namespace path_adapter {

std::string StorePathString(const std::filesystem::path &path);
std::string ResolveReadableFilePath(const std::filesystem::directory_entry &entry);
std::string ResolveReadableDirPath(const std::filesystem::path &path);

} // namespace path_adapter
