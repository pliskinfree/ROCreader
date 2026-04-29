#include "book_library_service.h"

#include "filesystem_compat.h"
#include "shelf_runtime.h"

#include <SDL.h>

#include <algorithm>
#include <set>
#include <sstream>
#include <unordered_map>

namespace {
void SortShelfItems(std::vector<BookItem> &items) {
  std::sort(items.begin(), items.end(), [](const BookItem &a, const BookItem &b) {
    if (a.is_dir != b.is_dir) return a.is_dir > b.is_dir;
    return a.name < b.name;
  });
}

bool PathMatchesAnyExt(const std::string &path, const std::vector<std::string> &wanted_exts,
                       const ShelfRuntimeDeps &deps) {
  const std::string ext = deps.get_lower_ext(path);
  for (const auto &wanted_ext : wanted_exts) {
    if (ext == wanted_ext) return true;
  }
  return false;
}

bool IsUnderRoot(const std::filesystem::path &path, const std::filesystem::path &root) {
  auto pit = path.begin();
  auto rit = root.begin();
  for (; rit != root.end(); ++rit, ++pit) {
    if (pit == path.end() || *pit != *rit) return false;
  }
  return true;
}

std::filesystem::path AncestorFromRawDocPath(const std::filesystem::path &raw_doc_path,
                                             size_t levels_up) {
  std::filesystem::path out = raw_doc_path;
  while (levels_up > 0 && !out.empty()) {
    out = out.parent_path();
    --levels_up;
  }
  return out;
}

std::vector<BookItem> BuildItemsFromScannedPaths(ShelfCategory category,
                                                 const std::string &current_folder,
                                                 const std::vector<std::string> &books_roots,
                                                 const ShelfRuntimeDeps &deps) {
  const std::vector<std::string> wanted_exts =
      (category == ShelfCategory::AllBooks)
          ? std::vector<std::string>{".txt"}
          : std::vector<std::string>{".pdf", ".epub", ".zip", ".cbz"};
  std::vector<BookItem> out;
  std::set<std::string> seen_files;
  std::set<std::string> seen_dirs;
  const std::vector<BookItem> scanned_books =
      deps.all_scanned_books ? deps.all_scanned_books() : std::vector<BookItem>{};

  auto add_file = [&](const BookItem &item) {
    const std::string key_path = item.real_path.empty() ? item.path : item.real_path;
    const std::string normalized = deps.normalize_path_key(key_path);
    if (!seen_files.insert(normalized).second) return;
    out.push_back(item);
  };
  auto add_dir = [&](const std::filesystem::path &p) {
    const std::string dir_path = p.string();
    const std::string normalized = deps.normalize_path_key(dir_path);
    if (!seen_dirs.insert(normalized).second) return;
    BookItem item;
    item.name = p.filename().string();
    item.path = dir_path;
    item.real_path = dir_path;
    item.native_fs_path = p;
    item.is_dir = true;
    out.push_back(std::move(item));
  };

  for (const auto &book : scanned_books) {
    const std::string source_path = book.real_path.empty() ? book.path : book.real_path;
    if (!PathMatchesAnyExt(source_path, wanted_exts, deps)) continue;
    const std::filesystem::path raw_doc_path =
        filesystem_compat::LexicallyNormal(std::filesystem::path(source_path));

    if (current_folder.empty()) {
      for (const auto &root_raw : books_roots) {
        const std::filesystem::path raw_root_path =
            filesystem_compat::LexicallyNormal(std::filesystem::path(root_raw));
        if (!IsUnderRoot(raw_doc_path, raw_root_path) || raw_doc_path == raw_root_path) continue;

        std::filesystem::path rel = filesystem_compat::LexicallyRelative(raw_doc_path, raw_root_path);
        if (rel.empty()) continue;
        const size_t rel_count = static_cast<size_t>(std::distance(rel.begin(), rel.end()));
        if (rel_count == 0) continue;
        if (rel_count == 1) {
          add_file(book);
        } else {
          add_dir(AncestorFromRawDocPath(raw_doc_path, rel_count - 1));
        }
        break;
      }
    } else {
      const std::filesystem::path raw_folder_path =
          filesystem_compat::LexicallyNormal(std::filesystem::path(current_folder));
      if (!IsUnderRoot(raw_doc_path, raw_folder_path) || raw_doc_path == raw_folder_path) continue;

      std::filesystem::path rel = filesystem_compat::LexicallyRelative(raw_doc_path, raw_folder_path);
      if (rel.empty()) continue;
      const size_t rel_count = static_cast<size_t>(std::distance(rel.begin(), rel.end()));
      if (rel_count == 0) continue;
      if (rel_count == 1) {
        add_file(book);
      } else {
        add_dir(AncestorFromRawDocPath(raw_doc_path, rel_count - 1));
      }
    }
  }

  SortShelfItems(out);
  return out;
}
}  // namespace

namespace book_library_service {

std::string MakeScanCacheKey(ShelfCategory category, const std::string &folder,
                             const std::vector<std::string> &books_roots,
                             const ShelfRuntimeDeps &deps) {
  std::ostringstream oss;
  oss << static_cast<int>(category) << "|";
  if (folder.empty()) {
    oss << "<root>";
    for (const auto &root : books_roots) oss << "|" << deps.normalize_path_key(root);
  } else {
    oss << deps.normalize_path_key(folder);
  }
  return oss.str();
}

void PruneScanCache(ShelfRuntimeState &state, size_t max_cache_entries) {
  while (state.scan_cache.size() > max_cache_entries) {
    auto oldest = state.scan_cache.end();
    for (auto it = state.scan_cache.begin(); it != state.scan_cache.end(); ++it) {
      if (oldest == state.scan_cache.end() || it->second.last_scan_tick < oldest->second.last_scan_tick) {
        oldest = it;
      }
    }
    if (oldest == state.scan_cache.end()) break;
    state.scan_cache.erase(oldest);
  }
}

bool MatchCategory(const BookItem &item, ShelfCategory category, const ShelfRuntimeDeps &deps) {
  if (item.is_dir) return category == ShelfCategory::AllComics || category == ShelfCategory::AllBooks;
  const std::string ext = deps.get_lower_ext(item.path);
  if (category == ShelfCategory::AllComics) {
    return ext == ".pdf" || ext == ".epub" || ext == ".zip" || ext == ".cbz";
  }
  if (category == ShelfCategory::AllBooks) return ext == ".txt";
  if (category == ShelfCategory::Collections) return deps.favorites_contains(item.path);
  if (category == ShelfCategory::History) return deps.history_contains(item.path);
  return true;
}

std::vector<BookItem> ScanBaseItems(ShelfRuntimeState &state, ShelfCategory category,
                                    const std::string &current_folder,
                                    const std::vector<std::string> &books_roots,
                                    const ShelfRuntimeDeps &deps) {
  const std::string cache_key = MakeScanCacheKey(category, current_folder, books_roots, deps);
  const uint32_t now = SDL_GetTicks();
  auto cache_it = state.scan_cache.find(cache_key);
  if (cache_it != state.scan_cache.end() && now - cache_it->second.last_scan_tick < deps.cache_ttl_ms) {
    return cache_it->second.items;
  }

  auto save_and_return = [&](std::vector<BookItem> out) {
    state.scan_cache[cache_key] = ShelfScanCacheEntry{out, now};
    PruneScanCache(state, deps.max_cache_entries);
    return out;
  };

  if (category == ShelfCategory::AllComics || category == ShelfCategory::AllBooks) {
    return save_and_return(BuildItemsFromScannedPaths(category, current_folder, books_roots, deps));
  }

  if (current_folder.empty() && category == ShelfCategory::Collections) {
    std::unordered_map<std::string, BookItem> found;
    const std::vector<BookItem> scanned_books =
        deps.all_scanned_books ? deps.all_scanned_books() : std::vector<BookItem>{};
    for (const auto &item : scanned_books) {
      const std::string key_path = item.real_path.empty() ? item.path : item.real_path;
      const std::string ext = deps.get_lower_ext(key_path);
      if (ext != ".pdf" && ext != ".txt" && ext != ".epub" && ext != ".zip" && ext != ".cbz") continue;
      found.emplace(deps.normalize_path_key(key_path), item);
    }
    std::vector<BookItem> out;
    for (const auto &path_key : deps.favorites_ordered_paths()) {
      auto it = found.find(path_key);
      if (it != found.end()) out.push_back(it->second);
    }
    return save_and_return(std::move(out));
  }

  if (current_folder.empty() && category == ShelfCategory::History) {
    std::unordered_map<std::string, BookItem> found;
    const std::vector<BookItem> scanned_books =
        deps.all_scanned_books ? deps.all_scanned_books() : std::vector<BookItem>{};
    for (const auto &item : scanned_books) {
      const std::string key_path = item.real_path.empty() ? item.path : item.real_path;
      const std::string ext = deps.get_lower_ext(key_path);
      if (ext != ".pdf" && ext != ".txt" && ext != ".epub" && ext != ".zip" && ext != ".cbz") continue;
      found.emplace(deps.normalize_path_key(key_path), item);
    }
    std::vector<BookItem> out;
    for (const auto &path_key : deps.history_ordered_paths()) {
      auto it = found.find(path_key);
      if (it != found.end()) out.push_back(it->second);
    }
    return save_and_return(std::move(out));
  }

  return save_and_return({});
}

void RebuildItems(ShelfRuntimeState &state, ShelfCategory category, const std::string &current_folder,
                  const std::vector<std::string> &books_roots, const ShelfRuntimeDeps &deps) {
  const std::vector<BookItem> base = ScanBaseItems(state, category, current_folder, books_roots, deps);
  state.items.clear();
  state.items.reserve(base.size());
  for (const auto &item : base) {
    if (MatchCategory(item, category, deps)) state.items.push_back(item);
  }
  ++state.content_version;
}

}  // namespace book_library_service
