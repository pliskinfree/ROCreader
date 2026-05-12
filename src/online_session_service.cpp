#include "online_session_service.h"

#include "filesystem_compat.h"

#include <algorithm>
#include <sstream>
#include <system_error>

namespace {
std::string SafeLocalFilename(std::string name) {
  if (name.empty()) name = "online_book";
  std::string safe;
  safe.reserve(name.size());
  for (char ch : name) {
    const unsigned char uch = static_cast<unsigned char>(ch);
    if (ch == '\\' || ch == '/' || ch == ':' || ch == '*' || ch == '?' || ch == '"' || ch == '<' ||
        ch == '>' || ch == '|' || uch < 32) {
      safe.push_back('_');
    } else {
      safe.push_back(ch);
    }
  }
  while (!safe.empty() && (safe.back() == ' ' || safe.back() == '.')) safe.pop_back();
  while (safe.find("__") != std::string::npos) safe.replace(safe.find("__"), 2, "_");
  if (safe.empty() || safe == "_") safe = "online_book";
  if (safe.size() > 160) safe.resize(160);
  while (!safe.empty() && (static_cast<unsigned char>(safe.back()) & 0xC0) == 0x80) safe.pop_back();
  if (safe.empty()) safe = "online_book";
  return safe;
}

std::string SafeAsciiLocalFilename(std::string name) {
  if (name.empty()) name = "online_book";
  std::string ascii;
  ascii.reserve(name.size());
  for (char ch : name) {
    const unsigned char uch = static_cast<unsigned char>(ch);
    if (uch >= 128 || ch == '\\' || ch == '/' || ch == ':' || ch == '*' || ch == '?' || ch == '"' || ch == '<' ||
        ch == '>' || ch == '|' || uch < 32) {
      ascii.push_back('_');
    } else {
      ascii.push_back(ch);
    }
  }
  while (!ascii.empty() && (ascii.back() == ' ' || ascii.back() == '.')) ascii.pop_back();
  while (ascii.find("__") != std::string::npos) ascii.replace(ascii.find("__"), 2, "_");
  if (ascii.empty() || ascii == "_") {
    std::ostringstream oss;
    oss << "book_" << std::hex << std::hash<std::string>{}(name);
    ascii = oss.str();
  }
  if (ascii.size() > 80) ascii.resize(80);
  return ascii;
}

std::filesystem::path MakeLocalBookDestPath(const std::filesystem::path &books_root,
                                            const OnlineCatalogItem &catalog_item,
                                            const std::filesystem::path &source_book) {
  const std::string ext = source_book.extension().string();
  try {
    return books_root / (SafeLocalFilename(catalog_item.title) + ext);
  } catch (const std::filesystem::filesystem_error &) {
  }
  return books_root / (SafeAsciiLocalFilename(catalog_item.title) + ext);
}

std::string CatalogKey(const OnlineCatalogItem &item) {
  if (!item.id.empty()) return item.id;
  return item.local_path.empty() ? item.title : item.local_path;
}

std::string BookKey(const BookItem &item) {
  if (!item.remote_id.empty()) return item.remote_id;
  return item.remote_local_path.empty() ? item.name : item.remote_local_path;
}

OnlineCatalogItem *FindCatalogItem(OnlineSourceState &state, const BookItem &item) {
  const std::string key = BookKey(item);
  for (OnlineCatalogItem &catalog_item : state.catalog_items) {
    if (CatalogKey(catalog_item) == key || catalog_item.local_path == item.remote_local_path ||
        catalog_item.local_path == item.real_path || catalog_item.title == item.name) {
      return &catalog_item;
    }
  }
  return nullptr;
}

const OnlineCatalogItem *FindCatalogItem(const OnlineSourceState &state, const BookItem &item) {
  const std::string key = BookKey(item);
  for (const OnlineCatalogItem &catalog_item : state.catalog_items) {
    if (CatalogKey(catalog_item) == key || catalog_item.local_path == item.remote_local_path ||
        catalog_item.local_path == item.real_path || catalog_item.title == item.name) {
      return &catalog_item;
    }
  }
  return nullptr;
}

std::filesystem::path FirstRootOrSibling(const std::vector<std::string> &roots,
                                         const std::filesystem::path &fallback) {
  for (const std::string &root : roots) {
    if (!root.empty()) return std::filesystem::path(root);
  }
  return fallback;
}

bool MoveOrCopyFile(const std::filesystem::path &from, const std::filesystem::path &to) {
  std::error_code ec;
  if (from.empty() || !std::filesystem::exists(from, ec) || ec) return false;
  std::filesystem::create_directories(to.parent_path(), ec);
  if (ec) return false;
  std::filesystem::rename(from, to, ec);
  if (!ec) return true;
  ec.clear();
  std::filesystem::copy_file(from, to, std::filesystem::copy_options::overwrite_existing, ec);
  if (ec) return false;
  ec.clear();
  std::filesystem::remove(from, ec);
  return true;
}
}  // namespace

bool MarkOnlineItemForLocal(OnlineSourceState &state, const BookItem &item) {
  OnlineCatalogItem *catalog_item = FindCatalogItem(state, item);
  if (!catalog_item) return false;
  catalog_item->marked_for_local = true;
  state.status_message = "Marked for local: " + catalog_item->title;
  return true;
}

bool UnmarkOnlineItemForLocal(OnlineSourceState &state, const BookItem &item) {
  OnlineCatalogItem *catalog_item = FindCatalogItem(state, item);
  if (!catalog_item) return false;
  catalog_item->marked_for_local = false;
  state.status_message = "Unmarked: " + catalog_item->title;
  return true;
}

bool OnlineItemMarkedForLocal(const OnlineSourceState &state, const BookItem &item) {
  const OnlineCatalogItem *catalog_item = FindCatalogItem(state, item);
  return catalog_item && catalog_item->marked_for_local;
}

std::vector<BookItem> BuildMarkedOnlineShelfItems(const OnlineSourceState &state) {
  return BuildOnlineShelfItems(state, true);
}

size_t CountMarkedOnlineItems(const OnlineSourceState &state) {
  return static_cast<size_t>(std::count_if(state.catalog_items.begin(), state.catalog_items.end(),
                                          [](const OnlineCatalogItem &item) { return item.marked_for_local; }));
}

OnlineMigrationResult MigrateMarkedOnlineItems(
    OnlineSourceState &state,
    const std::vector<std::string> &books_roots,
    const std::vector<std::string> &cover_roots,
    const std::function<void(size_t, size_t)> &on_progress) {
  OnlineMigrationResult result;
  result.total = CountMarkedOnlineItems(state);
  if (result.total == 0) return result;

  const std::filesystem::path fallback_books = state.download_root.parent_path() / "books";
  const std::filesystem::path books_root = FirstRootOrSibling(books_roots, fallback_books);
  const std::filesystem::path covers_root = FirstRootOrSibling(cover_roots, books_root.parent_path() / "book_covers");

  size_t done = 0;
  for (OnlineCatalogItem &catalog_item : state.catalog_items) {
    if (!catalog_item.marked_for_local) continue;
    if (on_progress) on_progress(done, result.total);
    state.status_message = "Moving " + std::to_string(done + 1) + "/" + std::to_string(result.total);

    const std::filesystem::path source_book(catalog_item.local_path);
    if (source_book.empty()) {
      ++result.failed;
      ++done;
      continue;
    }
    const std::filesystem::path dest_book = MakeLocalBookDestPath(books_root, catalog_item, source_book);
    const bool book_moved = MoveOrCopyFile(source_book, dest_book);

    BookItem remote_item;
    remote_item.name = catalog_item.title;
    remote_item.real_path = catalog_item.local_path;
    remote_item.remote_id = catalog_item.id;
    remote_item.remote_source_id = state.active_source_index >= 0 &&
                                           state.active_source_index < static_cast<int>(state.sources.size())
                                       ? state.sources[state.active_source_index].id
                                       : std::string();
    remote_item.remote_cover_url = catalog_item.cover_url;
    remote_item.remote_local_path = catalog_item.local_path;
    const std::filesystem::path source_cover = OnlineCoverPathForItem(state, remote_item);
    if (!source_cover.empty()) {
      const std::string cover_ext = source_cover.extension().empty() ? ".jpg" : source_cover.extension().string();
      const std::filesystem::path dest_cover = covers_root / (dest_book.stem().string() + cover_ext);
      (void)MoveOrCopyFile(source_cover, dest_cover);
    }

    if (book_moved) ++result.moved;
    else ++result.failed;
    catalog_item.marked_for_local = false;
    ++done;
    if (on_progress) on_progress(done, result.total);
  }
  state.status_message = "Moved " + std::to_string(result.moved) + "/" + std::to_string(result.total) +
                         " to local library";
  return result;
}

void DisconnectOnlineSourceWithMigration(OnlineSourceState &state,
                                         const std::vector<std::string> &books_roots,
                                         const std::vector<std::string> &cover_roots) {
  const size_t marked = CountMarkedOnlineItems(state);
  if (marked > 0) {
    MigrateMarkedOnlineItems(state, books_roots, cover_roots);
  }
  ClearOnlineSourceDownloads(state);
  DisconnectOnlineSource(state);
  state.status_message = marked > 0 ? "Online source disconnected, saved marked books"
                                    : "Online source disconnected";
}
