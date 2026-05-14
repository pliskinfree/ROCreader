#pragma once

#include "book_scanner.h"
#include "cover_cache_runtime.h"
#include "filesystem_compat.h"
#include "online_shelf_bridge.h"
#include "shelf_runtime.h"

#include <SDL.h>

#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

struct OnlineShelfControllerDeps {
  SDL_Renderer *renderer = nullptr;
  CoverCacheRuntime *cover_cache = nullptr;
  std::function<int()> cover_w;
  std::function<int()> cover_h;
  float cover_aspect = 0.0f;
  std::function<bool(const std::filesystem::path &)> file_exists;
  std::function<SDL_Surface *(const std::string &)> load_surface_from_file;
  std::function<SDL_Texture *(SDL_Renderer *, SDL_Surface *, int, int, float)> create_normalized_cover_texture;
  std::function<SDL_Texture *(SDL_Renderer *, SDL_Surface *)> create_texture_from_surface;
  std::function<void(SDL_Texture *, int, int)> remember_texture_size;
  std::function<void(SDL_Texture *, int &, int &)> get_texture_size;
  std::function<void(SDL_Texture *)> forget_texture_size;
  std::function<bool(const BookItem &)> open_local_item;
  std::function<void(const std::string &)> show_message;
};

struct OnlineShelfControllerTickResult {
  bool download_active = false;
  bool download_finished = false;
  bool download_success = false;
  bool online_connection_changed = false;
  bool online_shelf_needs_reset = false;
  bool refresh_roots_after_disconnect = false;
  bool shelf_items_changed = false;
  bool cover_cache_changed = false;
};

class OnlineShelfController {
public:
  OnlineShelfController();
  ~OnlineShelfController();

  OnlineSourceState &State();
  const OnlineSourceState &State() const;

  void Initialize(const std::filesystem::path &config_path, const std::filesystem::path &download_root);
  void Shutdown();

  bool IsActive() const;
  bool RebuildShelfIfActive(ShelfRuntimeState &shelf_runtime, int nav_selected_index);
  bool IsDownloadActive();

  SDL_Texture *GetCoverTexture(const BookItem &item, OnlineShelfControllerDeps &deps);
  SDL_Texture *GetCachedCoverTexture(const BookItem &item, OnlineShelfControllerDeps &deps);
  bool PreloadCoverTexture(const BookItem &item, OnlineShelfControllerDeps &deps);
  bool RemoteCoverLoading(const BookItem &item, const OnlineShelfControllerDeps &deps) const;
  std::string RemoteBookStatusText(const BookItem &item, const OnlineShelfControllerDeps &deps);
  float RemoteBookStatusProgress(const BookItem &item, const OnlineShelfControllerDeps &deps) const;

  bool OpenOrDownloadBook(const BookItem &item, const OnlineShelfControllerDeps &deps);
  bool MarkForLocal(const BookItem &item, const OnlineShelfControllerDeps &deps);
  bool UnmarkForLocal(const BookItem &item, const OnlineShelfControllerDeps &deps);

  int NavItemCount() const;
  std::string NavLabelText(int index) const;

  OnlineShelfControllerTickResult TickAfterInput(ShelfRuntimeState &shelf_runtime);
  OnlineShelfControllerTickResult TickAfterPresent(int &nav_selected_index, int &focus_index, int &shelf_page,
                                                   int grid_cols);
  bool HandleDeferredConnect();
  bool HasDeferredConnect() const;
  void HandleDeferredDisconnect(const std::vector<std::string> &books_roots,
                                const std::vector<std::string> &cover_roots);
  std::string StatusMessage() const;

private:
  struct DownloadJob {
    std::thread worker;
    mutable std::mutex mutex;
    bool active = false;
    bool finished = false;
    bool success = false;
    BookItem remote_item;
    BookItem local_item;
    std::string message;
  };
  struct CoverJob {
    std::thread worker;
    mutable std::mutex mutex;
    bool active = false;
    bool finished = false;
    bool success = false;
    std::vector<OnlineCatalogItem> items;
  };

  std::string BookKey(const BookItem &item) const;
  bool FileExists(const std::filesystem::path &path, const OnlineShelfControllerDeps &deps) const;
  bool BookDownloaded(const BookItem &item, const OnlineShelfControllerDeps &deps) const;
  bool BookDownloading(const BookItem &item) const;
  float BookDownloadProgress(const BookItem &item, const OnlineShelfControllerDeps &deps) const;
  BookItem LocalItemForRemote(const BookItem &item) const;
  OnlineShelfCoverDeps MakeCoverDeps(OnlineShelfControllerDeps &deps);
  void JoinDownloadJob();
  void JoinCoverJob();
  bool ProcessFocusedCoverWindow(int focus_index, int grid_cols);
  bool EnsureCatalogLookahead(int focus_index, int grid_cols);

  OnlineSourceState state_{};
  bool last_connected_ = false;
  DownloadJob download_job_;
  CoverJob cover_job_;
  std::unordered_map<std::string, std::string> book_status_text_;
  size_t last_cover_window_begin_ = 0;
  size_t last_cover_window_end_ = 0;
  size_t cover_window_cursor_ = 0;
  int cover_window_category_index_ = -1;
  size_t cover_window_catalog_size_ = 0;
};
