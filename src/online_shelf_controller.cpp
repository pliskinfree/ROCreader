#include "online_shelf_controller.h"

#include "online_session_service.h"
#include "online_source_runtime.h"
#include "online_source_transport.h"
#include "runtime_log.h"

#include <algorithm>
#include <array>
#include <fstream>

OnlineShelfController::OnlineShelfController() = default;

OnlineShelfController::~OnlineShelfController() {
  Shutdown();
}

OnlineSourceState &OnlineShelfController::State() {
  return state_;
}

const OnlineSourceState &OnlineShelfController::State() const {
  return state_;
}

void OnlineShelfController::Initialize(const std::filesystem::path &config_path,
                                       const std::filesystem::path &download_root) {
  InitializeOnlineSourceState(state_, config_path, download_root);
  last_connected_ = state_.connected;
}

void OnlineShelfController::Shutdown() {
  JoinDownloadJob();
  JoinCoverJob();
}

bool OnlineShelfController::IsActive() const {
  return OnlineShelfActive(state_);
}

bool OnlineShelfController::RebuildShelfIfActive(ShelfRuntimeState &shelf_runtime, int nav_selected_index) {
  return RebuildOnlineShelfIfActive(state_, shelf_runtime, nav_selected_index);
}

bool OnlineShelfController::IsDownloadActive() {
  {
    std::lock_guard<std::mutex> lock(download_job_.mutex);
    if (download_job_.active) return true;
  }
  {
    std::lock_guard<std::mutex> lock(cover_job_.mutex);
    if (cover_job_.active) return true;
  }
  return false;
}

OnlineShelfCoverDeps OnlineShelfController::MakeCoverDeps(OnlineShelfControllerDeps &deps) {
  return OnlineShelfCoverDeps{
      deps.renderer,
      state_,
      *deps.cover_cache,
      deps.cover_w,
      deps.cover_h,
      deps.cover_aspect,
      deps.file_exists,
      deps.load_surface_from_file,
      deps.create_normalized_cover_texture,
      deps.create_texture_from_surface,
      deps.remember_texture_size,
      deps.get_texture_size,
      deps.forget_texture_size,
  };
}

SDL_Texture *OnlineShelfController::GetCoverTexture(const BookItem &item, OnlineShelfControllerDeps &deps) {
  OnlineShelfCoverDeps cover_deps = MakeCoverDeps(deps);
  return GetOnlineShelfCoverTexture(item, cover_deps);
}

SDL_Texture *OnlineShelfController::GetCachedCoverTexture(const BookItem &item, OnlineShelfControllerDeps &deps) {
  OnlineShelfCoverDeps cover_deps = MakeCoverDeps(deps);
  return GetCachedOnlineShelfCoverTexture(item, cover_deps);
}

bool OnlineShelfController::PreloadCoverTexture(const BookItem &item, OnlineShelfControllerDeps &deps) {
  OnlineShelfCoverDeps cover_deps = MakeCoverDeps(deps);
  return PreloadOnlineShelfCoverTexture(item, cover_deps);
}

bool OnlineShelfController::RemoteCoverLoading(const BookItem &item, const OnlineShelfControllerDeps &deps) const {
  if (!item.is_remote) return false;
  return !FileExists(OnlineCoverPathForItem(state_, item), deps);
}

std::string OnlineShelfController::RemoteBookStatusText(const BookItem &item, const OnlineShelfControllerDeps &deps) {
  if (!item.is_remote) return {};
  const std::string key = BookKey(item);
  auto it = book_status_text_.find(key);
  if (it != book_status_text_.end()) return it->second;
  if (RemoteCoverLoading(item, deps)) return u8"\u52a0\u8f7d\u4e2d";
  return BookDownloaded(item, deps) ? u8"\u4e0b\u8f7d\u6210\u529f" : u8"\u672a\u4e0b\u8f7d";
}

float OnlineShelfController::RemoteBookStatusProgress(const BookItem &item, const OnlineShelfControllerDeps &deps) const {
  if (!item.is_remote) return 1.0f;
  if (BookDownloading(item)) return BookDownloadProgress(item, deps);
  if (!RemoteCoverLoading(item, deps)) return 1.0f;
  if (last_cover_window_end_ <= last_cover_window_begin_) return -1.0f;
  const size_t total = last_cover_window_end_ - last_cover_window_begin_;
  size_t done = 0;
  for (size_t i = last_cover_window_begin_; i < last_cover_window_end_ && i < state_.catalog_items.size(); ++i) {
    if (OnlineCatalogCoverExists(state_, state_.catalog_items[i])) ++done;
  }
  return total == 0 ? -1.0f : std::clamp(static_cast<float>(done) / static_cast<float>(total), 0.0f, 1.0f);
}

bool OnlineShelfController::OpenOrDownloadBook(const BookItem &item, const OnlineShelfControllerDeps &deps) {
  if (!item.is_remote) return deps.open_local_item ? deps.open_local_item(item) : false;
  if (BookDownloaded(item, deps)) {
    book_status_text_[BookKey(item)] = u8"\u4e0b\u8f7d\u6210\u529f";
    return deps.open_local_item ? deps.open_local_item(LocalItemForRemote(item)) : false;
  }

  const std::string key = BookKey(item);
  {
    std::lock_guard<std::mutex> lock(download_job_.mutex);
    if (download_job_.active) {
      if (BookKey(download_job_.remote_item) == key) {
        if (deps.show_message) deps.show_message(u8"\u4e0b\u8f7d\u4e2d...");
      } else {
        if (deps.show_message) deps.show_message(u8"\u5df2\u6709\u4e0b\u8f7d\u4efb\u52a1\u5728\u8fdb\u884c");
      }
      return false;
    }
    download_job_.active = true;
    download_job_.finished = false;
    download_job_.success = false;
    download_job_.remote_item = item;
    download_job_.local_item = BookItem{};
    download_job_.message.clear();
  }

  book_status_text_[key] = u8"\u4e0b\u8f7d\u4e2d";
  if (deps.show_message) deps.show_message(u8"\u4e0b\u8f7d\u4e2d...");
  OnlineSourceState state_snapshot = state_;
  JoinDownloadJob();
  download_job_.worker = std::thread([this, state_snapshot, item]() mutable {
    BookItem local_item;
    const bool ok = DownloadOnlineBookForItem(state_snapshot, item, local_item);
    std::lock_guard<std::mutex> lock(download_job_.mutex);
    download_job_.success = ok;
    download_job_.local_item = local_item;
    download_job_.message = state_snapshot.status_message;
    download_job_.finished = true;
  });
  return true;
}

bool OnlineShelfController::MarkForLocal(const BookItem &item, const OnlineShelfControllerDeps &deps) {
  const std::filesystem::path local_path = !item.remote_local_path.empty()
                                               ? std::filesystem::path(item.remote_local_path)
                                               : std::filesystem::path(item.real_path);
  if (!LocalZipComplete(local_path)) {
    if (deps.show_message) deps.show_message(u8"\u8bf7\u5148\u4e0b\u8f7d\u5b8c\u6210");
    state_.status_message = "Download first: " + item.name;
    return false;
  }
  const bool ok = MarkOnlineItemForLocal(state_, item);
  if (ok && deps.show_message) deps.show_message(u8"\u5df2\u6807\u8bb0\u4fdd\u5b58\u5230\u672c\u5730");
  if (!ok && deps.show_message) deps.show_message(u8"\u8bf7\u5148\u4e0b\u8f7d\u5b8c\u6210");
  return ok;
}

bool OnlineShelfController::UnmarkForLocal(const BookItem &item, const OnlineShelfControllerDeps &deps) {
  const bool ok = UnmarkOnlineItemForLocal(state_, item);
  if (ok && deps.show_message) deps.show_message(u8"\u5df2\u53d6\u6d88\u4fdd\u5b58\u6807\u8bb0");
  return ok;
}

void OnlineShelfController::StopOnlineBackgroundJobs() {
  CancelOnlineSourceTransfers();
  JoinCoverJob();
  JoinDownloadJob();
  {
    std::lock_guard<std::mutex> lock(cover_job_.mutex);
    cover_job_.active = false;
    cover_job_.finished = false;
    cover_job_.success = false;
    cover_job_.items.clear();
  }
  {
    std::lock_guard<std::mutex> lock(download_job_.mutex);
    download_job_.active = false;
    download_job_.finished = false;
    download_job_.success = false;
    download_job_.remote_item = BookItem{};
    download_job_.local_item = BookItem{};
    download_job_.message.clear();
  }
  state_.covers_loading = false;
  state_.cover_download_cursor = 0;
  last_cover_window_begin_ = 0;
  last_cover_window_end_ = 0;
  cover_window_cursor_ = 0;
  cover_window_category_index_ = -1;
  cover_window_catalog_size_ = 0;
}

int OnlineShelfController::NavItemCount() const {
  if (IsActive() && state_.active_source_index >= 0 &&
      state_.active_source_index < static_cast<int>(state_.sources.size())) {
    return std::max(1, static_cast<int>(state_.sources[state_.active_source_index].categories.size()) + 1);
  }
  return 4;
}

std::string OnlineShelfController::NavLabelText(int index) const {
  if (IsActive() && state_.active_source_index >= 0 &&
      state_.active_source_index < static_cast<int>(state_.sources.size())) {
    const auto &categories = state_.sources[state_.active_source_index].categories;
    if (index >= 0 && index < static_cast<int>(categories.size())) return categories[index].name;
    if (index == static_cast<int>(categories.size())) return u8"\u4fdd\u5b58\u5230\u672c\u5730";
  }
  static const std::array<std::string, 4> labels = {"ALL COMICS", "ALL BOOKS", "COLLECTIONS", "HISTORY"};
  if (index >= 0 && index < static_cast<int>(labels.size())) return labels[index];
  return {};
}

OnlineShelfControllerTickResult OnlineShelfController::TickAfterInput(ShelfRuntimeState &shelf_runtime) {
  OnlineShelfControllerTickResult result;

  {
    std::lock_guard<std::mutex> lock(download_job_.mutex);
    result.download_active = download_job_.active;
    if (download_job_.active && download_job_.finished) {
      result.download_finished = true;
      result.download_success = download_job_.success;
      const std::string key = BookKey(download_job_.remote_item);
      book_status_text_[key] = download_job_.success ? u8"\u4e0b\u8f7d\u6210\u529f" : u8"\u4e0b\u8f7d\u5931\u8d25";
      state_.status_message = download_job_.message;
      if (download_job_.success) {
        (void)SyncDownloadedOnlineItem(state_, download_job_.remote_item, download_job_.local_item);
        result.shelf_items_changed = true;
      }
      download_job_.active = false;
      result.download_active = false;
    }
  }
  if (result.download_finished) JoinDownloadJob();

  bool cover_finished_success = false;
  {
    std::lock_guard<std::mutex> lock(cover_job_.mutex);
    if (cover_job_.active && cover_job_.finished) {
      cover_job_.active = false;
      cover_finished_success = cover_job_.success;
      result.cover_cache_changed = true;
    }
  }
  if (result.cover_cache_changed) JoinCoverJob();
  if (cover_finished_success) result.shelf_items_changed = true;

  if (state_.disconnect_requested) {
    state_.disconnect_requested = false;
    result.refresh_roots_after_disconnect = true;
  }

  if (last_connected_ != state_.connected) {
    last_connected_ = state_.connected;
    last_cover_window_begin_ = 0;
    last_cover_window_end_ = 0;
    cover_window_cursor_ = 0;
    cover_window_category_index_ = state_.loaded_category_index;
    cover_window_catalog_size_ = state_.catalog_items.size();
    result.online_connection_changed = true;
    result.online_shelf_needs_reset = true;
    result.shelf_items_changed = true;
    result.cover_cache_changed = true;
  } else if (state_.connected && state_.loaded_category_index >= 0 && IsActive() && shelf_runtime.items.empty()) {
    result.shelf_items_changed = true;
  }

  return result;
}

OnlineShelfControllerTickResult OnlineShelfController::TickAfterPresent(int &nav_selected_index, int &focus_index,
                                                                        int &shelf_page, int grid_cols) {
  OnlineShelfControllerTickResult result;
  if (!IsActive()) return result;
  if (state_.pending_category_index >= 0) {
    const int pending_category = state_.pending_category_index;
    nav_selected_index = pending_category;
    focus_index = 0;
    shelf_page = 0;
    if (LoadOnlineCatalogForCategory(state_, pending_category)) {
      last_cover_window_begin_ = 0;
      last_cover_window_end_ = 0;
      cover_window_cursor_ = 0;
      cover_window_category_index_ = state_.loaded_category_index;
      cover_window_catalog_size_ = state_.catalog_items.size();
      result.cover_cache_changed = true;
      result.shelf_items_changed = true;
      result.online_shelf_needs_reset = true;
    } else {
      state_.pending_category_index = -1;
      state_.category_loading = false;
      result.shelf_items_changed = true;
    }
  } else {
    if (EnsureCatalogLookahead(focus_index, grid_cols)) {
      result.shelf_items_changed = true;
      result.cover_cache_changed = true;
    }
    if (ProcessFocusedCoverWindow(focus_index, grid_cols)) {
      result.cover_cache_changed = true;
    }
  }
  return result;
}

bool OnlineShelfController::HandleDeferredConnect() {
  if (!state_.connect_pending) return false;
  runtime_log::Line("online: deferred connect begin");
  state_.connecting = true;
  state_.status_message = "Connecting...";
  ConnectOnlineSource(state_);
  runtime_log::Line("online: deferred connect end connected=" + std::to_string(state_.connected ? 1 : 0) +
                    " items=" + std::to_string(state_.catalog_items.size()) +
                    " status=" + state_.status_message);
  return true;
}

bool OnlineShelfController::HasDeferredConnect() const {
  return state_.connect_pending;
}

void OnlineShelfController::HandleDeferredDisconnect(const std::vector<std::string> &books_roots,
                                                     const std::vector<std::string> &cover_roots) {
  StopOnlineBackgroundJobs();
  DisconnectOnlineSourceWithMigration(state_, books_roots, cover_roots);
}

std::string OnlineShelfController::StatusMessage() const {
  return state_.status_message;
}

std::string OnlineShelfController::BookKey(const BookItem &item) const {
  if (!item.remote_id.empty()) return item.remote_source_id + "|" + item.remote_id;
  return item.remote_source_id + "|" + item.remote_download_url + "|" + item.name;
}

bool OnlineShelfController::FileExists(const std::filesystem::path &path, const OnlineShelfControllerDeps &deps) const {
  return deps.file_exists ? deps.file_exists(path) : false;
}

bool OnlineShelfController::LocalZipComplete(const std::filesystem::path &path) const {
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
  if (!(sig[0] == 'P' && sig[1] == 'K' && ((sig[2] == 3 && sig[3] == 4) || (sig[2] == 5 && sig[3] == 6)))) {
    return false;
  }
  in.seekg(0, std::ios::end);
  const std::streamoff size = in.tellg();
  const std::streamoff start = std::max<std::streamoff>(0, size - 65536);
  in.seekg(start, std::ios::beg);
  std::string tail(static_cast<size_t>(size - start), '\0');
  in.read(&tail[0], static_cast<std::streamsize>(tail.size()));
  return tail.find("PK\005\006") != std::string::npos || tail.find("PK\006\006") != std::string::npos;
}

bool OnlineShelfController::BookDownloaded(const BookItem &item, const OnlineShelfControllerDeps &deps) const {
  if (!item.is_remote || item.remote_local_path.empty()) return false;
  return FileExists(std::filesystem::path(item.remote_local_path), deps);
}

bool OnlineShelfController::BookDownloading(const BookItem &item) const {
  const std::string key = BookKey(item);
  std::lock_guard<std::mutex> lock(download_job_.mutex);
  return download_job_.active && BookKey(download_job_.remote_item) == key;
}

float OnlineShelfController::BookDownloadProgress(const BookItem &item, const OnlineShelfControllerDeps &deps) const {
  if (!item.is_remote || item.remote_local_path.empty()) return -1.0f;
  const std::filesystem::path part_path = std::filesystem::path(item.remote_local_path).string() + ".part";
  const std::filesystem::path size_path = std::filesystem::path(item.remote_local_path).string() + ".size";
  std::error_code ec;
  const uintmax_t part_size = std::filesystem::exists(part_path, ec) && !ec ? std::filesystem::file_size(part_path, ec) : 0;
  if (part_size == 0 || ec) return -1.0f;
  uintmax_t total_size = 0;
  if (std::filesystem::exists(size_path, ec) && !ec) {
    std::ifstream in(size_path);
    if (in) in >> total_size;
  }
  if (total_size > 0) {
    const double progress = static_cast<double>(part_size) / static_cast<double>(total_size);
    return std::clamp(static_cast<float>(progress), 0.0f, 0.995f);
  }
  constexpr double kSoftFullBytes = 256.0 * 1024.0 * 1024.0;
  const double progress = static_cast<double>(part_size) / (static_cast<double>(part_size) + kSoftFullBytes);
  return std::clamp(static_cast<float>(progress), 0.02f, 0.92f);
}

BookItem OnlineShelfController::LocalItemForRemote(const BookItem &item) const {
  BookItem local_item = item;
  local_item.is_remote = false;
  local_item.path = item.remote_local_path;
  local_item.real_path = item.remote_local_path;
  local_item.native_fs_path = std::filesystem::path(item.remote_local_path);
  return local_item;
}

void OnlineShelfController::JoinDownloadJob() {
  if (download_job_.worker.joinable()) download_job_.worker.join();
}

void OnlineShelfController::JoinCoverJob() {
  if (cover_job_.worker.joinable()) cover_job_.worker.join();
}

bool OnlineShelfController::ProcessFocusedCoverWindow(int focus_index, int grid_cols) {
  if (!state_.connected || state_.catalog_items.empty() || grid_cols <= 0) return false;
  (void)focus_index;
  {
    std::lock_guard<std::mutex> lock(cover_job_.mutex);
    if (cover_job_.active) return false;
  }
  const size_t begin = 0;
  const size_t end = state_.catalog_items.size();
  if (begin != last_cover_window_begin_ || end != last_cover_window_end_ ||
      cover_window_cursor_ < begin || cover_window_cursor_ >= end ||
      cover_window_category_index_ != state_.loaded_category_index ||
      cover_window_catalog_size_ != state_.catalog_items.size()) {
    last_cover_window_begin_ = begin;
    last_cover_window_end_ = end;
    cover_window_cursor_ = begin;
    cover_window_category_index_ = state_.loaded_category_index;
    cover_window_catalog_size_ = state_.catalog_items.size();
  }
  if (begin >= end) return false;
  std::vector<OnlineCatalogItem> batch;
  batch.reserve(4);
  const size_t span = end - begin;
  for (size_t n = 0; n < span; ++n) {
    const size_t index = begin + ((cover_window_cursor_ - begin + n) % span);
    const OnlineCatalogItem &item = state_.catalog_items[index];
    if (!OnlineCatalogCoverExists(state_, item)) {
      cover_window_cursor_ = index + 1;
      if (cover_window_cursor_ >= end) cover_window_cursor_ = begin;
      batch.push_back(item);
      if (batch.size() >= 4) break;
    }
  }
  if (!batch.empty()) {
    state_.covers_loading = true;
    {
      std::lock_guard<std::mutex> lock(cover_job_.mutex);
      cover_job_.active = true;
      cover_job_.finished = false;
      cover_job_.success = false;
      cover_job_.items = batch;
    }
    OnlineSourceState state_snapshot = state_;
    JoinCoverJob();
    cover_job_.worker = std::thread([this, state_snapshot, batch]() mutable {
      bool any_ok = false;
      for (const OnlineCatalogItem &item : batch) {
        any_ok = DownloadOnlineCoverForCatalogItem(state_snapshot, item) || any_ok;
      }
      std::lock_guard<std::mutex> lock(cover_job_.mutex);
      cover_job_.success = any_ok;
      cover_job_.finished = true;
    });
    return false;
  }
  cover_window_cursor_ = begin;
  state_.covers_loading = false;
  return false;
}

bool OnlineShelfController::EnsureCatalogLookahead(int focus_index, int grid_cols) {
  if (!state_.connected || state_.next_page_url.empty() || grid_cols <= 0 || state_.catalog_items.empty()) return false;
  const int clamped_focus = std::clamp(focus_index, 0, static_cast<int>(state_.catalog_items.size()) - 1);
  const int focus_row = clamped_focus / grid_cols;
  const int loaded_rows = static_cast<int>((state_.catalog_items.size() + static_cast<size_t>(grid_cols) - 1) /
                                           static_cast<size_t>(grid_cols));
  // Keep at least two rows beyond the focused row, matching the cover prefetch window.
  if (loaded_rows - focus_row > 3) return false;
  return LoadOnlineCatalogNextPage(state_);
}
