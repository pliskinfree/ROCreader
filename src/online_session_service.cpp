#include "online_session_service.h"

#include "filesystem_compat.h"
#include "runtime_log.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <system_error>

namespace {
bool IsUtf8Continuation(unsigned char ch) {
  return (ch & 0xC0) == 0x80;
}

size_t Utf8SequenceLength(unsigned char ch) {
  if (ch < 0x80) return 1;
  if (ch >= 0xC2 && ch <= 0xDF) return 2;
  if (ch >= 0xE0 && ch <= 0xEF) return 3;
  if (ch >= 0xF0 && ch <= 0xF4) return 4;
  return 0;
}

bool IsValidUtf8Sequence(const std::string &text, size_t offset, size_t length) {
  if (length == 0 || offset + length > text.size()) return false;
  if (length == 1) return static_cast<unsigned char>(text[offset]) < 0x80;
  for (size_t i = 1; i < length; ++i) {
    if (!IsUtf8Continuation(static_cast<unsigned char>(text[offset + i]))) return false;
  }
  const unsigned char a = static_cast<unsigned char>(text[offset]);
  const unsigned char b = static_cast<unsigned char>(text[offset + 1]);
  if (a == 0xE0 && b < 0xA0) return false;
  if (a == 0xED && b >= 0xA0) return false;
  if (a == 0xF0 && b < 0x90) return false;
  if (a == 0xF4 && b >= 0x90) return false;
  return true;
}

std::string TrimTrailingFilenameChars(std::string text) {
  while (!text.empty() && (text.back() == ' ' || text.back() == '.')) text.pop_back();
  while (text.find("__") != std::string::npos) text.replace(text.find("__"), 2, "_");
  if (text.empty() || text == "_") text = "online_book";
  return text;
}

std::string Utf8SafeFilename(std::string name, size_t max_bytes) {
  if (name.empty()) name = "online_book";
  std::string safe;
  safe.reserve(std::min(name.size(), max_bytes));
  for (size_t i = 0; i < name.size();) {
    const unsigned char ch = static_cast<unsigned char>(name[i]);
    if (ch < 0x80) {
      const char ascii = static_cast<char>(ch);
      if (ascii == '\\' || ascii == '/' || ascii == ':' || ascii == '*' || ascii == '?' || ascii == '"' ||
          ascii == '<' || ascii == '>' || ascii == '|' || ch < 32) {
        if (safe.size() + 1 > max_bytes) break;
        safe.push_back('_');
      } else {
        if (safe.size() + 1 > max_bytes) break;
        safe.push_back(ascii);
      }
      ++i;
      continue;
    }

    const size_t length = Utf8SequenceLength(ch);
    if (!IsValidUtf8Sequence(name, i, length)) {
      if (safe.size() + 1 > max_bytes) break;
      safe.push_back('_');
      ++i;
      continue;
    }
    if (safe.size() + length > max_bytes) break;
    safe.append(name, i, length);
    i += length;
  }
  return TrimTrailingFilenameChars(std::move(safe));
}

std::string SafeLocalFilename(std::string name) {
  return Utf8SafeFilename(std::move(name), 160);
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
  ascii = TrimTrailingFilenameChars(std::move(ascii));
  if (ascii.empty() || ascii == "_") {
    std::ostringstream oss;
    oss << "book_" << std::hex << std::hash<std::string>{}(name);
    ascii = oss.str();
  }
  if (ascii.size() > 80) ascii.resize(80);
  return ascii;
}

std::string ErrorText(const std::error_code &ec) {
  if (!ec) return "ok";
  return std::to_string(ec.value()) + ":" + ec.message();
}

std::filesystem::path MakeLocalBookDestPath(const std::filesystem::path &books_root,
                                            const OnlineCatalogItem &catalog_item,
                                            const std::filesystem::path &source_book) {
  const std::string ext = source_book.extension().string();
  std::filesystem::path base_path;
  try {
    base_path = books_root / (SafeLocalFilename(catalog_item.title) + ext);
  } catch (const std::filesystem::filesystem_error &) {
    base_path = books_root / (SafeAsciiLocalFilename(catalog_item.title) + ext);
  }
  std::error_code ec;
  if (!std::filesystem::exists(base_path, ec) || ec) return base_path;
  const std::string stem = base_path.stem().string();
  const std::string suffix = base_path.extension().string();
  for (int n = 2; n < 1000; ++n) {
    std::filesystem::path candidate = base_path.parent_path() / (stem + " (" + std::to_string(n) + ")" + suffix);
    ec.clear();
    if (!std::filesystem::exists(candidate, ec) || ec) return candidate;
  }
  return base_path.parent_path() / (stem + "_" + std::to_string(std::hash<std::string>{}(catalog_item.local_path)) + suffix);
}

std::string CatalogKey(const OnlineCatalogItem &item) {
  if (!item.id.empty()) return item.id;
  return item.local_path.empty() ? item.title : item.local_path;
}

std::string HashText(const std::string &text) {
  std::ostringstream oss;
  oss << std::hex << std::hash<std::string>{}(text);
  return oss.str();
}

OnlineCatalogItem MakeMarkedLocalItemSnapshot(const OnlineCatalogItem &item) {
  OnlineCatalogItem snapshot = item;
  snapshot.marked_for_local = true;
  return snapshot;
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
  for (OnlineCatalogItem &catalog_item : state.marked_local_items) {
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
  for (const OnlineCatalogItem &catalog_item : state.marked_local_items) {
    if (CatalogKey(catalog_item) == key || catalog_item.local_path == item.remote_local_path ||
        catalog_item.local_path == item.real_path || catalog_item.title == item.name) {
      return &catalog_item;
    }
  }
  return nullptr;
}

bool PathContainsDownloadsDir(const std::filesystem::path &path) {
  for (const auto &part : path) {
    if (part == "Downloads") return true;
  }
  return false;
}

std::filesystem::path SelectMigrationBooksRoot(const OnlineSourceState &state,
                                               const std::vector<std::string> &books_roots) {
  const std::filesystem::path preferred = state.download_root.parent_path() / "books";
  if (!preferred.empty() && !PathContainsDownloadsDir(preferred)) {
    return preferred;
  }
  for (const std::string &root : books_roots) {
    if (root.empty()) continue;
    const std::filesystem::path candidate(root);
    if (PathContainsDownloadsDir(candidate)) {
      runtime_log::Line("online: migration skip downloads books root=" + candidate.string());
      continue;
    }
    return candidate;
  }
  return preferred;
}

std::filesystem::path SelectMigrationCoversRoot(const std::filesystem::path &books_root,
                                                const std::vector<std::string> &cover_roots) {
  const std::filesystem::path preferred = books_root.parent_path() / "book_covers";
  if (!preferred.empty() && !PathContainsDownloadsDir(preferred)) {
    return preferred;
  }
  for (const std::string &root : cover_roots) {
    if (root.empty()) continue;
    const std::filesystem::path candidate(root);
    if (PathContainsDownloadsDir(candidate)) {
      runtime_log::Line("online: migration skip downloads cover root=" + candidate.string());
      continue;
    }
    return candidate;
  }
  return preferred;
}

bool MoveOrCopyFile(const std::filesystem::path &from, const std::filesystem::path &to) {
  std::error_code ec;
  if (from.empty() || !std::filesystem::exists(from, ec) || ec) {
    runtime_log::Line("online: move source missing from=" + from.string() + " error=" + ErrorText(ec));
    return false;
  }
  std::filesystem::create_directories(to.parent_path(), ec);
  if (ec) {
    runtime_log::Line("online: move mkdir failed dir=" + to.parent_path().string() +
                      " error=" + ErrorText(ec));
    return false;
  }
  std::filesystem::rename(from, to, ec);
  if (!ec) return true;
  const std::string rename_error = ErrorText(ec);
  ec.clear();
  std::filesystem::copy_file(from, to, std::filesystem::copy_options::overwrite_existing, ec);
  if (ec) {
    const std::string copy_file_error = ErrorText(ec);
    ec.clear();
    std::ifstream in(from, std::ios::binary);
    std::ofstream out(to, std::ios::binary | std::ios::trunc);
    if (!in || !out) {
      runtime_log::Line("online: move copy open failed from=" + from.string() + " to=" + to.string() +
                        " rename_error=" + rename_error + " copy_file_error=" + copy_file_error);
      return false;
    }
    out << in.rdbuf();
    out.close();
    in.close();
    if (!out) {
      runtime_log::Line("online: move stream copy failed from=" + from.string() + " to=" + to.string() +
                        " rename_error=" + rename_error + " copy_file_error=" + copy_file_error);
      return false;
    }
    ec.clear();
    if (!std::filesystem::exists(to, ec) || ec) {
      runtime_log::Line("online: move stream copy missing dest from=" + from.string() + " to=" + to.string() +
                        " rename_error=" + rename_error + " copy_file_error=" + copy_file_error +
                        " exists_error=" + ErrorText(ec));
      return false;
    }
  }
  ec.clear();
  std::filesystem::remove(from, ec);
  if (ec) {
    runtime_log::Line("online: move copied but remove failed from=" + from.string() +
                      " to=" + to.string() + " error=" + ErrorText(ec));
  }
  return true;
}

bool LooksLikeCompleteZip(const std::filesystem::path &path) {
  std::error_code ec;
  if (path.empty() || !std::filesystem::exists(path, ec) || ec ||
      !std::filesystem::is_regular_file(path, ec) || ec ||
      std::filesystem::file_size(path, ec) < 22 || ec) {
    return false;
  }
  std::ifstream in(path, std::ios::binary);
  if (!in) return false;
  char sig[4] = {};
  in.read(sig, sizeof(sig));
  if (in.gcount() != static_cast<std::streamsize>(sizeof(sig))) return false;
  const bool local_header = sig[0] == 'P' && sig[1] == 'K' && sig[2] == 3 && sig[3] == 4;
  const bool empty_zip = sig[0] == 'P' && sig[1] == 'K' && sig[2] == 5 && sig[3] == 6;
  if (!local_header && !empty_zip) return false;
  const std::streamoff tail_len = 65536;
  in.seekg(0, std::ios::end);
  const std::streamoff size = in.tellg();
  const std::streamoff start = std::max<std::streamoff>(0, size - tail_len);
  in.seekg(start, std::ios::beg);
  std::string tail(static_cast<size_t>(size - start), '\0');
  in.read(&tail[0], static_cast<std::streamsize>(tail.size()));
  return tail.find("PK\005\006") != std::string::npos || tail.find("PK\006\006") != std::string::npos;
}

std::filesystem::path ExpectedDownloadPathForCatalogItem(const OnlineSourceState &state,
                                                         const OnlineCatalogItem &catalog_item) {
  if (state.active_source_index < 0 || state.active_source_index >= static_cast<int>(state.sources.size())) {
    return {};
  }
  const OnlineSourceEntry &source = state.sources[state.active_source_index];
  const std::string key = HashText(catalog_item.id.empty() ? catalog_item.download_url + catalog_item.title
                                                           : catalog_item.id);
  const std::string base = "book_" + key.substr(0, std::min<size_t>(12, key.size()));
  return state.download_root / "books" / SafeAsciiLocalFilename(source.id) / (base + catalog_item.file_ext);
}

std::filesystem::path ResolveDownloadedBookPath(const OnlineSourceState &state,
                                                const OnlineCatalogItem &catalog_item) {
  const std::string key = CatalogKey(catalog_item);
  std::vector<std::filesystem::path> candidates;
  if (!catalog_item.local_path.empty()) candidates.emplace_back(catalog_item.local_path);
  for (const OnlineCatalogItem &active_item : state.catalog_items) {
    if (CatalogKey(active_item) == key || active_item.title == catalog_item.title) {
      if (!active_item.local_path.empty()) candidates.emplace_back(active_item.local_path);
      const std::filesystem::path expected = ExpectedDownloadPathForCatalogItem(state, active_item);
      if (!expected.empty()) candidates.push_back(expected);
    }
  }
  const std::filesystem::path expected = ExpectedDownloadPathForCatalogItem(state, catalog_item);
  if (!expected.empty()) candidates.push_back(expected);
  for (const std::filesystem::path &candidate : candidates) {
    if (LooksLikeCompleteZip(candidate)) return candidate;
  }
  const std::filesystem::path books_dir = state.download_root / "books";
  std::error_code ec;
  if (std::filesystem::exists(books_dir, ec) && !ec) {
    for (std::filesystem::recursive_directory_iterator it(books_dir, ec), end; !ec && it != end; it.increment(ec)) {
      if (std::filesystem::is_regular_file(it->path(), ec) && !ec && it->path().extension() == catalog_item.file_ext &&
          LooksLikeCompleteZip(it->path())) {
        if (!catalog_item.local_path.empty() && it->path().filename() == std::filesystem::path(catalog_item.local_path).filename()) {
          runtime_log::Line("online: migrate fallback matched filename title=" + catalog_item.title +
                            " path=" + it->path().string());
          return it->path();
        }
      }
    }
  }
  return {};
}
}  // namespace

bool MarkOnlineItemForLocal(OnlineSourceState &state, const BookItem &item) {
  OnlineCatalogItem *catalog_item = FindCatalogItem(state, item);
  if (!catalog_item) return false;
  const std::filesystem::path downloaded_path(catalog_item->local_path.empty() ? item.remote_local_path
                                                                               : catalog_item->local_path);
  if (!LooksLikeCompleteZip(downloaded_path)) {
    state.status_message = "Download first: " + catalog_item->title;
    runtime_log::Line("online: mark local rejected, missing complete zip title=" + catalog_item->title +
                      " path=" + downloaded_path.string());
    return false;
  }
  catalog_item->marked_for_local = true;
  state.marked_local_keys.insert(CatalogKey(*catalog_item));
  const std::string key = CatalogKey(*catalog_item);
  auto it = std::find_if(state.marked_local_items.begin(), state.marked_local_items.end(),
                         [&](const OnlineCatalogItem &marked_item) { return CatalogKey(marked_item) == key; });
  if (it == state.marked_local_items.end()) {
    state.marked_local_items.push_back(MakeMarkedLocalItemSnapshot(*catalog_item));
  } else {
    *it = MakeMarkedLocalItemSnapshot(*catalog_item);
  }
  state.status_message = "Marked for local: " + catalog_item->title;
  return true;
}

bool UnmarkOnlineItemForLocal(OnlineSourceState &state, const BookItem &item) {
  OnlineCatalogItem *catalog_item = FindCatalogItem(state, item);
  if (!catalog_item) return false;
  catalog_item->marked_for_local = false;
  const std::string key = CatalogKey(*catalog_item);
  state.marked_local_keys.erase(key);
  state.marked_local_items.erase(std::remove_if(state.marked_local_items.begin(), state.marked_local_items.end(),
                                                [&](const OnlineCatalogItem &marked_item) {
                                                  return CatalogKey(marked_item) == key;
                                                }),
                                 state.marked_local_items.end());
  state.status_message = "Unmarked: " + catalog_item->title;
  return true;
}

bool OnlineItemMarkedForLocal(const OnlineSourceState &state, const BookItem &item) {
  const OnlineCatalogItem *catalog_item = FindCatalogItem(state, item);
  return catalog_item && (catalog_item->marked_for_local ||
                          state.marked_local_keys.find(CatalogKey(*catalog_item)) != state.marked_local_keys.end());
}

bool SyncDownloadedOnlineItem(OnlineSourceState &state, const BookItem &remote_item, const BookItem &local_item) {
  OnlineCatalogItem *catalog_item = FindCatalogItem(state, remote_item);
  if (!catalog_item) catalog_item = FindCatalogItem(state, local_item);
  if (!catalog_item) {
    runtime_log::Line("online: downloaded item sync failed title=" + remote_item.name);
    return false;
  }
  if (!local_item.real_path.empty()) {
    catalog_item->local_path = local_item.real_path;
  } else if (!local_item.path.empty()) {
    catalog_item->local_path = local_item.path;
  } else if (!remote_item.remote_local_path.empty()) {
    catalog_item->local_path = remote_item.remote_local_path;
  }
  runtime_log::Line("online: downloaded item synced title=" + catalog_item->title +
                    " path=" + catalog_item->local_path);
  const std::string key = CatalogKey(*catalog_item);
  auto marked_it = std::find_if(state.marked_local_items.begin(), state.marked_local_items.end(),
                                [&](const OnlineCatalogItem &marked_item) { return CatalogKey(marked_item) == key; });
  if (marked_it != state.marked_local_items.end()) *marked_it = MakeMarkedLocalItemSnapshot(*catalog_item);
  return true;
}

std::vector<BookItem> BuildMarkedOnlineShelfItems(const OnlineSourceState &state) {
  std::vector<BookItem> out;
  if (!state.connected || state.active_source_index < 0 ||
      state.active_source_index >= static_cast<int>(state.sources.size())) {
    return out;
  }
  const OnlineSourceEntry &source = state.sources[state.active_source_index];
  const std::vector<OnlineCatalogItem> &items = !state.marked_local_items.empty() ? state.marked_local_items
                                                                                  : state.catalog_items;
  for (const OnlineCatalogItem &remote : items) {
    const std::string mark_key = remote.id.empty() ? (remote.local_path.empty() ? remote.title : remote.local_path)
                                                   : remote.id;
    const bool marked = remote.marked_for_local ||
                        state.marked_local_keys.find(mark_key) != state.marked_local_keys.end();
    if (!marked) continue;
    BookItem item;
    item.name = remote.title;
    item.path = remote.local_path.empty() ? remote.title : remote.local_path;
    item.real_path = item.path;
    if (!remote.local_path.empty()) item.native_fs_path = std::filesystem::path(remote.local_path);
    item.is_dir = false;
    item.is_remote = true;
    item.remote_id = remote.id;
    item.remote_source_id = source.id;
    item.remote_cover_url = remote.cover_url;
    item.remote_download_url = remote.download_url;
    item.remote_local_path = remote.local_path;
    out.push_back(std::move(item));
  }
  return out;
}

size_t CountMarkedOnlineItems(const OnlineSourceState &state) {
  if (!state.marked_local_items.empty()) {
    return static_cast<size_t>(std::count_if(state.marked_local_items.begin(), state.marked_local_items.end(),
                                            [&](const OnlineCatalogItem &item) {
                                              return item.marked_for_local ||
                                                     state.marked_local_keys.find(CatalogKey(item)) !=
                                                         state.marked_local_keys.end();
                                            }));
  }
  return static_cast<size_t>(std::count_if(state.catalog_items.begin(), state.catalog_items.end(),
                                          [&](const OnlineCatalogItem &item) {
                                            return item.marked_for_local ||
                                                   state.marked_local_keys.find(CatalogKey(item)) !=
                                                       state.marked_local_keys.end();
                                          }));
}

OnlineMigrationResult MigrateMarkedOnlineItems(
    OnlineSourceState &state,
    const std::vector<std::string> &books_roots,
    const std::vector<std::string> &cover_roots,
    const std::function<void(size_t, size_t)> &on_progress) {
  OnlineMigrationResult result;
  result.total = CountMarkedOnlineItems(state);
  if (result.total == 0) return result;

  const std::filesystem::path books_root = SelectMigrationBooksRoot(state, books_roots);
  const std::filesystem::path covers_root = SelectMigrationCoversRoot(books_root, cover_roots);
  runtime_log::Line("online: migration roots books=" + books_root.string() +
                    " covers=" + covers_root.string() +
                    " download_root=" + state.download_root.string());

  size_t done = 0;
  const std::vector<OnlineCatalogItem> migration_items = !state.marked_local_items.empty() ? state.marked_local_items
                                                                                           : state.catalog_items;
  for (OnlineCatalogItem catalog_item : migration_items) {
    if (!catalog_item.marked_for_local &&
        state.marked_local_keys.find(CatalogKey(catalog_item)) == state.marked_local_keys.end()) {
      continue;
    }
    if (on_progress) on_progress(done, result.total);
    state.status_message = "Moving " + std::to_string(done + 1) + "/" + std::to_string(result.total);

    const std::filesystem::path source_book = ResolveDownloadedBookPath(state, catalog_item);
    std::error_code ec;
    if (source_book.empty() || !std::filesystem::exists(source_book, ec) || ec) {
      runtime_log::Line("online: migrate marked missing downloaded file title=" + catalog_item.title +
                        " catalog_path=" + catalog_item.local_path + " resolved_path=" + source_book.string());
      ++result.failed;
      ++done;
      continue;
    }
    catalog_item.local_path = source_book.string();
    const std::filesystem::path dest_book = MakeLocalBookDestPath(books_root, catalog_item, source_book);
    runtime_log::Line("online: migrate marked begin title=" + catalog_item.title +
                      " source=" + source_book.string() +
                      " dest=" + dest_book.string());
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

    if (book_moved) {
      ++result.moved;
      runtime_log::Line("online: migrate marked moved title=" + catalog_item.title +
                        " source=" + source_book.string() + " dest=" + dest_book.string());
      catalog_item.marked_for_local = false;
      const std::string key = CatalogKey(catalog_item);
      state.marked_local_keys.erase(key);
      for (OnlineCatalogItem &active_item : state.catalog_items) {
        if (CatalogKey(active_item) == key) active_item.marked_for_local = false;
      }
      state.marked_local_items.erase(std::remove_if(state.marked_local_items.begin(), state.marked_local_items.end(),
                                                    [&](const OnlineCatalogItem &marked_item) {
                                                      return CatalogKey(marked_item) == key;
                                                    }),
                                     state.marked_local_items.end());
    } else {
      ++result.failed;
      runtime_log::Line("online: migrate marked failed title=" + catalog_item.title +
                        " source=" + source_book.string() + " dest=" + dest_book.string());
    }
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
  OnlineMigrationResult migration;
  if (marked > 0) migration = MigrateMarkedOnlineItems(state, books_roots, cover_roots);
  if (migration.failed == 0) {
    ClearOnlineSourceDownloads(state);
  } else {
    runtime_log::Line("online: cache clear skipped after migration failures failed=" +
                      std::to_string(migration.failed));
  }
  DisconnectOnlineSource(state);
  if (marked > 0 && migration.failed > 0) {
    state.status_message = "Online source disconnected, saved " + std::to_string(migration.moved) +
                           "/" + std::to_string(migration.total) + " marked books";
  } else {
    state.status_message = marked > 0 ? "Online source disconnected, saved marked books"
                                      : "Online source disconnected";
  }
}
