#pragma once

#include "book_scanner.h"

#include <cstddef>
#include <string>
#include <vector>

enum class ShelfCategory;
struct ShelfRuntimeDeps;
struct ShelfRuntimeState;

namespace book_library_service {

std::string MakeScanCacheKey(ShelfCategory category, const std::string &folder,
                             const std::vector<std::string> &books_roots,
                             const ShelfRuntimeDeps &deps);
void PruneScanCache(ShelfRuntimeState &state, size_t max_cache_entries);
bool MatchCategory(const BookItem &item, ShelfCategory category, const ShelfRuntimeDeps &deps);
std::vector<BookItem> ScanBaseItems(ShelfRuntimeState &state, ShelfCategory category,
                                    const std::string &current_folder,
                                    const std::vector<std::string> &books_roots,
                                    const ShelfRuntimeDeps &deps);
void RebuildItems(ShelfRuntimeState &state, ShelfCategory category, const std::string &current_folder,
                  const std::vector<std::string> &books_roots, const ShelfRuntimeDeps &deps);

}  // namespace book_library_service
