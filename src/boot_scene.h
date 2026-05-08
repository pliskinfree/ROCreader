#pragma once

#include "boot_runtime.h"
#include "shelf_scene.h"
#include "ui_text_cache.h"

#include <SDL.h>

#include <functional>
#include <string>
#include <vector>

struct BookItem;

struct BootSceneTickDeps {
  const std::vector<std::string> &books_roots;
  size_t count_batch_entries = 0;
  size_t scan_batch_entries = 0;
  size_t cover_generate_batch_entries = 0;
  size_t cover_preload_batch_entries = 0;
  std::function<std::string(const std::string &)> get_lower_ext;
  std::function<bool(const std::string &)> doc_cover_backend_available;
  std::function<bool(const BookItem &)> has_manual_cover_exact_or_fuzzy;
  std::function<bool(const std::string &)> has_cached_doc_cover_on_disk;
  std::function<SDL_Texture *(const std::string &)> create_doc_first_page_cover_texture;
  std::function<void(SDL_Texture *)> destroy_generated_cover_texture;
  std::function<std::vector<BookItem>()> build_shelf_cover_preload_items;
  std::function<void(const BookItem &)> preload_shelf_cover_texture;
  std::function<bool()> install_pending_update;
  std::function<void()> on_update_installed_restart;
  std::function<void(size_t, size_t)> on_finish;
};

struct BootSceneRenderDeps {
  SDL_Renderer *renderer = nullptr;
  int language_index = 0;
  int screen_w = 0;
  int screen_h = 0;
  std::function<void(int, int, int, int, SDL_Color, bool)> draw_rect;
  std::function<TextCacheEntry *(const std::string &, SDL_Color)> get_text_texture;
};

struct BootSceneShelfResetDeps {
  ShelfSceneState &shelf_state;
  float title_marquee_pause_sec = 0.0f;
};

struct BootSceneFinishDeps {
  BootSceneShelfResetDeps shelf_reset;
  std::function<void()> rebuild_shelf_items;
  std::function<void()> reset_shelf_cover_stream_preload;
  std::function<void()> enter_shelf;
  bool verbose_log = false;
};

class BootScene {
public:
  explicit BootScene(BootRuntimeState &runtime, std::function<void()> request_quit = {});

  void Tick(float dt, const BootSceneTickDeps &deps);
  void Draw(const BootSceneRenderDeps &deps) const;

  bool InstallPendingUpdateFromEnvironment() const;
  void RestartAfterInstalledUpdate() const;
  void ResetShelfAfterBoot(BootSceneShelfResetDeps deps) const;
  void FinishScanAndEnterShelf(size_t total_books, size_t cover_generate_count, BootSceneFinishDeps deps) const;

private:
  BootRuntimeState &runtime_;
  std::function<void()> request_quit_;
};
