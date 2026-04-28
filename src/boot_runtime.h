#pragma once

#include "book_scanner.h"
#include "ui_text_cache.h"

#include <SDL.h>

#include "filesystem_compat.h"
#include <functional>
#include <future>
#include <string>
#include <unordered_set>
#include <vector>

enum class BootPhase { UpdateInstall, UpdateReplay, CountBooks, ScanBooks, GenerateCovers, Finalize, Done };

struct BootRuntimeState {
  float timer = 0.0f;
  BootPhase phase = BootPhase::CountBooks;
  int language_index = 0;
  std::vector<BookItem> scanned_books;
  std::unordered_set<std::string> scanned_book_keys;
  size_t scan_index = 0;
  std::vector<std::string> cover_generate_queue;
  size_t cover_generate_index = 0;
  size_t total_books = 0;
  std::filesystem::recursive_directory_iterator count_it;
  std::filesystem::recursive_directory_iterator count_end;
  size_t count_root_index = 0;
  bool count_iterator_active = false;
  std::string status_text = "Loading resources...(0/0)";
  bool update_replay_success = false;
  bool update_install_started = false;
  bool update_install_done = false;
  bool update_install_success = false;
  std::future<bool> update_install_future;
  std::string update_replay_version;
  std::filesystem::path update_status_path;
};

struct BootRuntimeTickDeps {
  const std::vector<std::string> &books_roots;
  size_t count_batch_entries = 0;
  size_t scan_batch_entries = 0;
  size_t cover_generate_batch_entries = 0;
  std::function<std::string(const std::string &)> get_lower_ext;
  std::function<bool(const std::string &)> doc_cover_backend_available;
  std::function<bool(const BookItem &)> has_manual_cover_exact_or_fuzzy;
  std::function<bool(const std::string &)> has_cached_doc_cover_on_disk;
  std::function<SDL_Texture *(const std::string &)> create_doc_first_page_cover_texture;
  std::function<void(SDL_Texture *)> destroy_generated_cover_texture;
  std::function<bool()> install_pending_update;
  std::function<void()> on_update_installed_restart;
  std::function<void(size_t, size_t)> on_finish;
};

struct BootRuntimeRenderDeps {
  SDL_Renderer *renderer = nullptr;
  const BootRuntimeState &state;
  int language_index = 0;
  int screen_w = 0;
  int screen_h = 0;
  std::function<void(int, int, int, int, SDL_Color, bool)> draw_rect;
  std::function<TextCacheEntry *(const std::string &, SDL_Color)> get_text_texture;
};

void TickBootRuntime(BootRuntimeState &state, float dt, const BootRuntimeTickDeps &deps);
void DrawBootRuntime(const BootRuntimeRenderDeps &deps);
void InitializeBootRuntimeReplay(BootRuntimeState &state, const std::filesystem::path &status_path);
void InitializeBootRuntimePendingUpdate(BootRuntimeState &state);
