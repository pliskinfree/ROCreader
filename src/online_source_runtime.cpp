#include "online_source_runtime.h"

#include "online_manual_web_provider.h"
#include "online_opds_provider.h"
#include "online_source_transport.h"
#include "runtime_log.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cctype>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <system_error>
#include <thread>

namespace {
std::string Trim(std::string text) {
  auto is_space = [](unsigned char ch) { return std::isspace(ch) != 0; };
  while (!text.empty() && is_space(static_cast<unsigned char>(text.front()))) text.erase(text.begin());
  while (!text.empty() && is_space(static_cast<unsigned char>(text.back()))) text.pop_back();
  return text;
}

std::string StripInlineComment(const std::string &line) {
  bool in_quote = false;
  for (size_t i = 0; i < line.size(); ++i) {
    const char ch = line[i];
    if (ch == '"') in_quote = !in_quote;
    if (!in_quote && (ch == '#' || ch == ';')) return line.substr(0, i);
  }
  return line;
}

bool StartsWith(const std::string &text, const std::string &prefix) {
  return text.rfind(prefix, 0) == 0;
}

uintmax_t ParseUnsignedFileValue(const std::filesystem::path &path) {
  std::ifstream in(path);
  uintmax_t value = 0;
  if (in) in >> value;
  return value;
}

void WriteDownloadProgressSidecar(const std::filesystem::path &progress_path,
                                  uintmax_t downloaded,
                                  uintmax_t total) {
  if (progress_path.empty()) return;
  const std::filesystem::path temp_path = progress_path.string() + ".tmp";
  std::ofstream out(temp_path, std::ios::trunc);
  if (!out) return;
  out << downloaded << ' ' << total << '\n';
  out.close();
  std::error_code ec;
  std::filesystem::rename(temp_path, progress_path, ec);
  if (ec) {
    ec.clear();
    std::filesystem::copy_file(temp_path, progress_path, std::filesystem::copy_options::overwrite_existing, ec);
    std::filesystem::remove(temp_path, ec);
  }
}

class DownloadProgressMonitor {
 public:
  DownloadProgressMonitor(std::filesystem::path part_path,
                          std::filesystem::path size_path,
                          std::filesystem::path progress_path)
      : part_path_(std::move(part_path)),
        size_path_(std::move(size_path)),
        progress_path_(std::move(progress_path)) {
    worker_ = std::thread([this]() { Run(); });
  }

  ~DownloadProgressMonitor() {
    Stop();
  }

  void Stop() {
    bool expected = false;
    if (stopped_.compare_exchange_strong(expected, true)) {
      if (worker_.joinable()) worker_.join();
      std::error_code ec;
      std::filesystem::remove(progress_path_, ec);
      std::filesystem::remove(progress_path_.string() + ".tmp", ec);
    }
  }

 private:
  void Run() {
    while (!stopped_.load()) {
      std::error_code ec;
      uintmax_t downloaded = 0;
      if (std::filesystem::exists(part_path_, ec) && !ec) {
        downloaded = std::filesystem::file_size(part_path_, ec);
        if (ec) downloaded = 0;
      }
      const uintmax_t total = ParseUnsignedFileValue(size_path_);
      if (downloaded > 0 || total > 0) {
        WriteDownloadProgressSidecar(progress_path_, downloaded, total);
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
  }

  std::filesystem::path part_path_;
  std::filesystem::path size_path_;
  std::filesystem::path progress_path_;
  std::atomic_bool stopped_{false};
  std::thread worker_;
};

std::string ToLowerAscii(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(),
                 [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  return text;
}

std::string SafeFilename(std::string name) {
  name = Trim(name);
  if (name.empty()) name = "online_book";
  std::string ascii;
  ascii.reserve(name.size());
  for (char &ch : name) {
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
  if (ascii.empty() || ascii == "_") ascii = "online_book";
  if (ascii.size() > 80) ascii.resize(80);
  return ascii;
}

std::string HashText(const std::string &text) {
  std::ostringstream oss;
  oss << std::hex << std::hash<std::string>{}(text);
  return oss.str();
}

std::string DefaultOnlineSourceConfigText() {
  return
      "# ROCreader online source configuration\n"
      "# Path: Roms/APPS/ROCreader/online_sources.ini\n"
      "# Downloads directory: Roms/APPS/ROCreader/Downloads\n"
      "# Release packages ship only this template file.\n"
      "# To add a new connection source:\n"
      "# 1. Copy the whole [source.template] block below.\n"
      "# 2. Rename the copied section, for example [source.my_site].\n"
      "# 3. Fill name, type, url, and category.* fields.\n"
      "# 4. Set visible=1 and enabled=1 to show it in the URL menu.\n"
      "# type supports: opds, kavita, and manual_web.\n"
      "\n"
      "[source.template]\n"
      "name=Template only - copy this block and fill a real source\n"
      "type=opds\n"
      "url=\n"
      "visible=0\n"
      "enabled=0\n"
      "category.0.name=All\n"
      "category.0.url=\n"
      "category.1.name=Category Name\n"
      "category.1.url=\n";
}

void EnsureDefaultConfigExists(const std::filesystem::path &config_path) {
  if (config_path.empty()) return;
  std::error_code ec;
  if (std::filesystem::exists(config_path, ec) && !ec) return;
  std::filesystem::create_directories(config_path.parent_path(), ec);
  std::ofstream out(config_path, std::ios::trunc);
  if (!out) return;
  out << DefaultOnlineSourceConfigText();
}

struct ParsedSource {
  OnlineSourceEntry entry;
  std::map<int, OnlineSourceCategory> categories;
};

bool ParseBool(std::string value, bool fallback) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  value = Trim(value);
  if (value == "1" || value == "true" || value == "yes" || value == "on") return true;
  if (value == "0" || value == "false" || value == "no" || value == "off") return false;
  return fallback;
}

bool ParseCategoryKey(const std::string &key, int &index, std::string &field) {
  constexpr const char *prefix = "category.";
  if (!StartsWith(key, prefix)) return false;
  const size_t number_start = std::string(prefix).size();
  const size_t dot = key.find('.', number_start);
  if (dot == std::string::npos || dot == number_start) return false;
  try {
    index = std::stoi(key.substr(number_start, dot - number_start));
  } catch (...) {
    return false;
  }
  field = key.substr(dot + 1);
  return index >= 0 && !field.empty();
}

std::vector<OnlineSourceEntry> LoadSources(const std::filesystem::path &config_path) {
  std::ifstream in(config_path);
  if (!in) return {};

  std::vector<ParsedSource> parsed;
  ParsedSource *current = nullptr;
  std::string line;
  while (std::getline(in, line)) {
    line = Trim(StripInlineComment(line));
    if (line.empty()) continue;
    if (line.front() == '[' && line.back() == ']') {
      const std::string section = Trim(line.substr(1, line.size() - 2));
      if (StartsWith(section, "source.")) {
        ParsedSource source;
        source.entry.id = section.substr(std::string("source.").size());
        source.entry.name = source.entry.id;
        parsed.push_back(std::move(source));
        current = &parsed.back();
      } else {
        current = nullptr;
      }
      continue;
    }
    if (!current) continue;
    const size_t eq = line.find('=');
    if (eq == std::string::npos) continue;
    const std::string key = Trim(line.substr(0, eq));
    const std::string value = Trim(line.substr(eq + 1));
    int category_index = -1;
    std::string category_field;
    if (ParseCategoryKey(key, category_index, category_field)) {
      OnlineSourceCategory &category = current->categories[category_index];
      if (category_field == "name") category.name = value;
      else if (category_field == "url") category.url = value;
      continue;
    }
    if (key == "name") current->entry.name = value;
    else if (key == "url") current->entry.url = value;
    else if (key == "type") current->entry.type = value;
    else if (key == "visible" || key == "show") current->entry.visible = ParseBool(value, current->entry.visible);
    else if (key == "enabled") current->entry.enabled = ParseBool(value, current->entry.enabled);
  }

  std::vector<OnlineSourceEntry> out;
  for (ParsedSource &source : parsed) {
    if (!source.entry.visible) continue;
    if (!source.entry.enabled) continue;
    if (source.entry.id.empty() || source.entry.url.empty()) continue;
    for (auto &pair : source.categories) {
      OnlineSourceCategory category = pair.second;
      if (category.name.empty()) category.name = "Category " + std::to_string(pair.first);
      if (!category.url.empty()) source.entry.categories.push_back(std::move(category));
    }
    if (source.entry.categories.empty()) {
      source.entry.categories.push_back(OnlineSourceCategory{"All", source.entry.url});
    }
    out.push_back(std::move(source.entry));
  }
  return out;
}
}  // namespace

void InitializeOnlineSourceState(OnlineSourceState &state,
                                 const std::filesystem::path &config_path,
                                 const std::filesystem::path &download_root) {
  state.config_path = config_path;
  state.download_root = download_root;
  std::error_code ec;
  std::filesystem::create_directories(state.download_root, ec);
  EnsureDefaultConfigExists(state.config_path);
  ReloadOnlineSourceConfig(state);
}

void ReloadOnlineSourceConfig(OnlineSourceState &state) {
  state.sources = LoadSources(state.config_path);
  if (state.sources.empty()) {
    state.selected_source_index = -1;
    state.active_source_index = -1;
    state.connected = false;
    state.status_message = "No online sources in online_sources.ini";
    return;
  }
  state.selected_source_index = std::clamp(state.selected_source_index, 0,
                                           static_cast<int>(state.sources.size()) - 1);
  state.selected_row = std::clamp(state.selected_row, 0, static_cast<int>(state.sources.size()));
  state.status_message = "Loaded " + std::to_string(state.sources.size()) + " online source(s)";
}

void ConnectOnlineSource(OnlineSourceState &state) {
  runtime_log::Line("online: connect requested");
  ResetOnlineSourceTransferCancel();
  state.connect_pending = false;
  state.connecting = true;
  if (state.sources.empty()) {
    state.connecting = false;
    state.connected = false;
    state.selected_source_index = -1;
    state.active_source_index = -1;
    state.status_message = "No source selected";
    return;
  }
  if (state.selected_source_index < 0 ||
      state.selected_source_index >= static_cast<int>(state.sources.size())) {
    state.connecting = false;
    state.connected = false;
    state.active_source_index = -1;
    state.status_message = "No source selected";
    return;
  }
  state.selected_source_index = std::clamp(state.selected_source_index, 0,
                                           static_cast<int>(state.sources.size()) - 1);
  const OnlineSourceEntry &source = state.sources[state.selected_source_index];
  runtime_log::Line("online: selected source id=" + source.id + " type=" + source.type + " url=" + source.url +
                    " enabled=" + std::to_string(source.enabled ? 1 : 0));
  if (!source.enabled) {
    state.connecting = false;
    state.connected = false;
    state.selected_source_index = -1;
    state.active_source_index = -1;
    state.status_message = "Selected source is disabled";
    return;
  }
  state.active_source_index = state.selected_source_index;
  state.connected = true;
  state.loaded_category_index = -1;
  state.pending_category_index = -1;
  state.category_loading = false;
  state.cover_download_cursor = 0;
  state.covers_loading = false;
  state.catalog_items.clear();
  const std::string source_type = ToLowerAscii(source.type);
  if (source_type != "opds" && source_type != "kavita" && source_type != "manual_web") {
    state.connecting = false;
    state.connected = false;
    state.active_source_index = -1;
    state.status_message = "Unsupported source type: " + source.type;
    return;
  }
  if (LoadOnlineCatalogForCategory(state, 0)) {
    state.connecting = false;
    state.status_message = "Connected: " + source.name + " (" + std::to_string(state.catalog_items.size()) + ")";
    runtime_log::Line("online: connected item_count=" + std::to_string(state.catalog_items.size()));
  } else {
    state.connecting = false;
    state.connected = false;
    state.active_source_index = -1;
    state.loaded_category_index = -1;
    state.loaded_category_url.clear();
    state.next_page_url.clear();
    state.next_page_loading = false;
    state.catalog_items.clear();
    state.status_message = "Catalog load failed: " + source.name;
    runtime_log::Line("online: catalog load failed source=" + source.name);
  }
}

void DisconnectOnlineSource(OnlineSourceState &state) {
  state.connect_pending = false;
  state.connecting = false;
  state.connected = false;
  state.disconnect_requested = false;
  state.selected_source_index = -1;
  state.active_source_index = -1;
  state.loaded_category_index = -1;
  state.loaded_category_url.clear();
  state.next_page_url.clear();
  state.next_page_loading = false;
  state.pending_category_index = -1;
  state.category_loading = false;
  state.cover_download_cursor = 0;
  state.covers_loading = false;
  state.catalog_items.clear();
  state.status_message = "Online source disconnected";
}

void ClearOnlineSourceDownloads(OnlineSourceState &state) {
  std::error_code ec;
  std::filesystem::create_directories(state.download_root / "books", ec);
  ec.clear();
  std::filesystem::create_directories(state.download_root / "book_covers", ec);
  if (ec) {
    state.status_message = "Cannot create Downloads directory";
    return;
  }
  std::filesystem::remove_all(state.download_root / "books", ec);
  ec.clear();
  std::filesystem::remove_all(state.download_root / "book_covers", ec);
  ec.clear();
  std::filesystem::create_directories(state.download_root / "books", ec);
  ec.clear();
  std::filesystem::create_directories(state.download_root / "book_covers", ec);
  state.status_message = "Downloads cache cleared";
}

bool LoadOnlineCatalogForCategory(OnlineSourceState &state, int category_index) {
  if (OnlineSourceTransfersCancelled()) return false;
  if (!state.connected || state.active_source_index < 0 ||
      state.active_source_index >= static_cast<int>(state.sources.size())) {
    return false;
  }
  const OnlineSourceEntry &source = state.sources[state.active_source_index];
  if (source.categories.empty()) return false;
  state.category_loading = true;
  state.pending_category_index = -1;
  category_index = std::clamp(category_index, 0, static_cast<int>(source.categories.size()) - 1);
  const std::string url = source.categories[category_index].url.empty() ? source.url : source.categories[category_index].url;
  runtime_log::Line("online: load catalog category=" + std::to_string(category_index) + " url=" + url);
  const std::string source_type = ToLowerAscii(source.type);
  std::string body;
  ManualWebCatalogPage manual_page;
  if (source_type == "manual_web") {
    manual_page = LoadManualWebCatalogPage(url, source.url);
    body = manual_page.body;
  } else {
    body = HttpGetText(url, source.url);
  }
  if (body.empty()) {
    state.catalog_items.clear();
    state.loaded_category_index = -1;
    state.category_loading = false;
    state.covers_loading = false;
    std::cerr << "[online] empty response url=" << url << "\n";
    runtime_log::Line("online: empty catalog response url=" + url);
    return false;
  }
  state.catalog_items = source_type == "manual_web" ? std::move(manual_page.items)
                                                    : ParseOpdsCatalog(body, url);
  state.loaded_category_url = url;
  state.next_page_url = source_type == "manual_web" ? manual_page.next_page_url : std::string();
  state.next_page_loading = false;
  runtime_log::Line("online: catalog response bytes=" + std::to_string(body.size()) +
                    " parsed_items=" + std::to_string(state.catalog_items.size()));
  if (state.catalog_items.empty()) {
    std::cerr << "[online] no parsed catalog entries url=" << url << " bytes=" << body.size() << "\n";
  }
  state.loaded_category_index = category_index;
  state.category_loading = false;
  state.cover_download_cursor = 0;
  state.covers_loading = !state.catalog_items.empty();
  for (OnlineCatalogItem &item : state.catalog_items) {
    const std::string key = HashText(item.id.empty() ? item.download_url + item.title : item.id);
    const std::string base = "book_" + key.substr(0, std::min<size_t>(12, key.size()));
    const std::filesystem::path local_path =
        state.download_root / "books" / SafeFilename(source.id) / (base + item.file_ext);
    item.local_path = local_path.string();
    const std::string mark_key = item.id.empty() ? (item.local_path.empty() ? item.title : item.local_path) : item.id;
    item.marked_for_local = state.marked_local_keys.find(mark_key) != state.marked_local_keys.end();
  }
  return !state.catalog_items.empty();
}

bool LoadOnlineCatalogNextPage(OnlineSourceState &state) {
  if (OnlineSourceTransfersCancelled()) return false;
  if (!state.connected || state.next_page_loading || state.next_page_url.empty() ||
      state.active_source_index < 0 || state.active_source_index >= static_cast<int>(state.sources.size())) {
    return false;
  }
  const OnlineSourceEntry &source = state.sources[state.active_source_index];
  const std::string source_type = ToLowerAscii(source.type);
  if (source_type != "manual_web") return false;
  const std::string url = state.next_page_url;
  state.next_page_loading = true;
  runtime_log::Line("online: load catalog next page url=" + url);
  ManualWebCatalogPage manual_page = LoadManualWebCatalogPage(url, source.url);
  const std::string &body = manual_page.body;
  state.next_page_loading = false;
  if (body.empty()) {
    runtime_log::Line("online: empty next catalog response url=" + url);
    state.next_page_url.clear();
    return false;
  }
  std::vector<OnlineCatalogItem> next_items = std::move(manual_page.items);
  const size_t before = state.catalog_items.size();
  std::set<std::string> seen;
  for (const OnlineCatalogItem &item : state.catalog_items) {
    seen.insert(item.id.empty() ? item.download_url : item.id);
  }
  for (OnlineCatalogItem &item : next_items) {
    const std::string item_key = item.id.empty() ? item.download_url : item.id;
    if (!seen.insert(item_key).second) continue;
    const std::string key = HashText(item.id.empty() ? item.download_url + item.title : item.id);
    const std::string base = "book_" + key.substr(0, std::min<size_t>(12, key.size()));
    const std::filesystem::path local_path =
        state.download_root / "books" / SafeFilename(source.id) / (base + item.file_ext);
    item.local_path = local_path.string();
    const std::string mark_key = item.id.empty() ? (item.local_path.empty() ? item.title : item.local_path) : item.id;
    item.marked_for_local = state.marked_local_keys.find(mark_key) != state.marked_local_keys.end();
    state.catalog_items.push_back(std::move(item));
  }
  state.next_page_url = manual_page.next_page_url;
  state.covers_loading = true;
  runtime_log::Line("online: next catalog response bytes=" + std::to_string(body.size()) +
                    " added_items=" + std::to_string(state.catalog_items.size() - before) +
                    " total_items=" + std::to_string(state.catalog_items.size()) +
                    " next=" + state.next_page_url);
  return state.catalog_items.size() > before;
}

std::vector<BookItem> BuildOnlineShelfItems(const OnlineSourceState &state, bool marked_only) {
  std::vector<BookItem> out;
  if (!state.connected || state.active_source_index < 0 ||
      state.active_source_index >= static_cast<int>(state.sources.size())) {
    return out;
  }
  const OnlineSourceEntry &source = state.sources[state.active_source_index];
  for (const OnlineCatalogItem &remote : state.catalog_items) {
    const std::string mark_key = remote.id.empty() ? (remote.local_path.empty() ? remote.title : remote.local_path)
                                                   : remote.id;
    const bool marked = remote.marked_for_local ||
                        state.marked_local_keys.find(mark_key) != state.marked_local_keys.end();
    if (marked_only && !marked) continue;
    BookItem item;
    item.name = remote.title;
    item.path = remote.local_path.empty() ? remote.title : remote.local_path;
    item.real_path = item.path;
    if (!remote.local_path.empty()) item.native_fs_path = std::filesystem::path(remote.local_path);
    item.is_dir = false;
    item.origin = remote.local_path.empty() ? BookOrigin::OnlineRemote : BookOrigin::OnlineDownloaded;
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

std::filesystem::path OnlineCoverPathForItem(const OnlineSourceState &state, const BookItem &item) {
  const std::string key = HashText(item.remote_id.empty() ? item.remote_cover_url + item.name : item.remote_id);
  const std::string base = "cover_" + key.substr(0, std::min<size_t>(12, key.size()));
  return state.download_root / "book_covers" / SafeFilename(item.remote_source_id) / (base + ".jpg");
}

bool DownloadOnlineCoverForItem(const OnlineSourceState &state, const BookItem &item) {
  if (OnlineSourceTransfersCancelled()) return false;
  if (!item.is_remote || item.remote_cover_url.empty()) return false;
  const std::filesystem::path cover_path = OnlineCoverPathForItem(state, item);
  std::error_code ec;
  if (std::filesystem::exists(cover_path, ec) && !ec) return true;
  const std::filesystem::path temp_path = cover_path.string() + ".part";
  std::filesystem::remove(temp_path, ec);
  if (state.active_source_index >= 0 && state.active_source_index < static_cast<int>(state.sources.size()) &&
      ToLowerAscii(state.sources[state.active_source_index].type) == "manual_web") {
    if (DownloadManualWebFile(item.remote_cover_url, temp_path, state.sources[state.active_source_index].url)) {
      std::filesystem::rename(temp_path, cover_path, ec);
      if (!ec) return true;
      ec.clear();
      std::filesystem::copy_file(temp_path, cover_path, std::filesystem::copy_options::overwrite_existing, ec);
      std::filesystem::remove(temp_path, ec);
      if (std::filesystem::exists(cover_path, ec) && !ec) return true;
    }
    std::filesystem::remove(temp_path, ec);
    runtime_log::Line("online: manual web cover helper failed; no generic fallback title=" + item.name +
                      " url=" + item.remote_cover_url);
    return false;
  }
  const bool ok = DownloadFile(item.remote_cover_url, temp_path);
  if (ok) {
    std::filesystem::rename(temp_path, cover_path, ec);
    if (ec) {
      ec.clear();
      std::filesystem::copy_file(temp_path, cover_path, std::filesystem::copy_options::overwrite_existing, ec);
      std::filesystem::remove(temp_path, ec);
    }
  }
  const bool saved = std::filesystem::exists(cover_path, ec) && !ec;
  if (!saved) {
    std::filesystem::remove(temp_path, ec);
    runtime_log::Line("online: cover download failed title=" + item.name + " url=" + item.remote_cover_url);
  }
  return saved;
}

bool OnlineCatalogCoverExists(const OnlineSourceState &state, const OnlineCatalogItem &catalog_item) {
  if (catalog_item.cover_url.empty()) return true;
  BookItem item;
  item.name = catalog_item.title;
  item.origin = BookOrigin::OnlineRemote;
  item.is_remote = true;
  item.remote_id = catalog_item.id;
  item.remote_cover_url = catalog_item.cover_url;
  item.remote_source_id = state.active_source_index >= 0 &&
                                  state.active_source_index < static_cast<int>(state.sources.size())
                              ? state.sources[state.active_source_index].id
                              : std::string();
  const std::filesystem::path cover_path = OnlineCoverPathForItem(state, item);
  std::error_code ec;
  return std::filesystem::exists(cover_path, ec) && !ec;
}

bool DownloadOnlineCoverForCatalogItem(const OnlineSourceState &state, const OnlineCatalogItem &catalog_item) {
  if (OnlineSourceTransfersCancelled()) return false;
  if (catalog_item.cover_url.empty()) return false;
  BookItem item;
  item.name = catalog_item.title;
  item.origin = BookOrigin::OnlineRemote;
  item.is_remote = true;
  item.remote_id = catalog_item.id;
  item.remote_cover_url = catalog_item.cover_url;
  item.remote_source_id = state.active_source_index >= 0 &&
                                  state.active_source_index < static_cast<int>(state.sources.size())
                              ? state.sources[state.active_source_index].id
                              : std::string();
  return DownloadOnlineCoverForItem(state, item);
}

bool DownloadOnlineBookForItem(OnlineSourceState &state, const BookItem &item, BookItem &out_local_item) {
  if (OnlineSourceTransfersCancelled()) {
    state.status_message = "Download cancelled";
    return false;
  }
  runtime_log::Line("online: book download requested title=" + item.name);
  if (!item.is_remote || item.remote_download_url.empty() || item.remote_local_path.empty()) {
    state.status_message = "No remote download URL";
    return false;
  }
  std::string download_url = item.remote_download_url;
  std::string referer;
  bool manual_web_source = false;
  std::string expected_size;
  if (state.active_source_index >= 0 && state.active_source_index < static_cast<int>(state.sources.size())) {
    const OnlineSourceEntry &source = state.sources[state.active_source_index];
    if (ToLowerAscii(source.type) == "manual_web") {
      manual_web_source = true;
      state.status_message = "Resolving download: " + item.name;
      const std::string real_url = ResolveManualWebDownloadUrl(item.remote_download_url, item.name, source.url);
      if (real_url.empty()) {
        state.status_message = "Download link failed: " + item.name;
        runtime_log::Line("online: manual web download link failed title=" + item.name +
                          " detail_url=" + item.remote_download_url);
        return false;
      }
      referer = item.remote_download_url;
      download_url = real_url;
    }
  }
  const std::filesystem::path final_path(item.remote_local_path);
  const std::filesystem::path temp_path = final_path.string() + ".part";
  const std::filesystem::path size_path = final_path.string() + ".size";
  std::error_code ec;
  if (std::filesystem::exists(final_path, ec) && !ec) {
    out_local_item = item;
    out_local_item.is_remote = false;
    out_local_item.origin = BookOrigin::OnlineDownloaded;
    out_local_item.path = final_path.string();
    out_local_item.real_path = final_path.string();
    out_local_item.native_fs_path = final_path;
    return true;
  }
  std::filesystem::remove(temp_path, ec);
  std::filesystem::remove(size_path, ec);
  expected_size = ProbeDownloadSize(download_url, referer);
  if (!expected_size.empty()) {
    std::ofstream size_out(size_path, std::ios::trunc);
    if (size_out) size_out << expected_size;
    runtime_log::Line("online: book download expected size title=" + item.name +
                      " bytes=" + expected_size);
  } else {
    runtime_log::Line("online: book download expected size unavailable title=" + item.name);
  }
  const std::filesystem::path progress_path = temp_path.string() + ".progress";
  DownloadProgressMonitor progress_monitor(temp_path, size_path, progress_path);
  const bool downloaded = manual_web_source ? DownloadManualWebFile(download_url, temp_path, referer)
                                            : DownloadFile(download_url, temp_path, referer);
  progress_monitor.Stop();
  if (!downloaded || OnlineSourceTransfersCancelled()) {
    std::filesystem::remove(progress_path, ec);
    std::filesystem::remove(temp_path, ec);
    std::filesystem::remove(size_path, ec);
    state.status_message = OnlineSourceTransfersCancelled() ? "Download cancelled: " + item.name
                                                            : "Download failed: " + item.name;
    runtime_log::Line("online: book download failed title=" + item.name + " url=" + download_url +
                      " cancelled=" + std::to_string(OnlineSourceTransfersCancelled() ? 1 : 0));
    return false;
  }
  std::filesystem::rename(temp_path, final_path, ec);
  if (ec) {
    std::filesystem::copy_file(temp_path, final_path, std::filesystem::copy_options::overwrite_existing, ec);
    std::filesystem::remove(temp_path, ec);
  }
  if (!std::filesystem::exists(final_path, ec) || ec) {
    state.status_message = "Save failed: " + item.name;
    runtime_log::Line("online: book save failed title=" + item.name + " path=" + final_path.string());
    std::filesystem::remove(progress_path, ec);
    std::filesystem::remove(size_path, ec);
    return false;
  }
  std::filesystem::remove(progress_path, ec);
  std::filesystem::remove(size_path, ec);
  out_local_item = item;
  out_local_item.is_remote = false;
  out_local_item.origin = BookOrigin::OnlineDownloaded;
  out_local_item.path = final_path.string();
  out_local_item.real_path = final_path.string();
  out_local_item.native_fs_path = final_path;
  state.status_message = "Downloaded: " + item.name;
  runtime_log::Line("online: book downloaded title=" + item.name + " path=" + final_path.string());
  return true;
}
