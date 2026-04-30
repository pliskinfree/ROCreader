#pragma once

#include "book_scanner.h"
#include "reader_core.h"

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

enum class ShelfCategory;
class ProgressStore;
class PathSetStore;
class RecentPathStore;
struct ShelfRuntimeDeps;
struct ShelfRuntimeState;

namespace book_library_service {

std::string MakeScanCacheKey(ShelfCategory category, const std::string &folder,
                             const std::vector<std::string> &books_roots,
                             const ShelfRuntimeDeps &deps);
void PruneScanCache(ShelfRuntimeState &state, size_t max_cache_entries);
bool MatchCategory(const BookItem &item, ShelfCategory category, const ShelfRuntimeDeps &deps);
const std::string &RealPathForItem(const BookItem &item);
ReaderProgress CompatibleProgressForItem(const BookItem &item, const ProgressStore &progress_store);
ShelfRuntimeDeps MakeShelfRuntimeDeps(
    std::function<std::string(const std::string &)> normalize_path_key,
    std::function<std::string(const std::string &)> get_lower_ext,
    std::function<std::vector<BookItem>()> all_scanned_books,
    const RecentPathStore &favorites_store,
    const RecentPathStore &history_store,
    uint32_t cache_ttl_ms,
    size_t max_cache_entries);
std::vector<BookItem> ScanBaseItems(ShelfRuntimeState &state, ShelfCategory category,
                                    const std::string &current_folder,
                                    const std::vector<std::string> &books_roots,
                                    const ShelfRuntimeDeps &deps);
void RebuildItems(ShelfRuntimeState &state, ShelfCategory category, const std::string &current_folder,
                  const std::vector<std::string> &books_roots, const ShelfRuntimeDeps &deps);

}  // namespace book_library_service
