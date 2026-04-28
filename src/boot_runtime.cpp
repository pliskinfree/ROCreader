#include "boot_runtime.h"
#include "app_language.h"
#include "path_adapter.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <future>
#include <chrono>

namespace {
constexpr float kUpdateReplayDurationSec = 1.8f;

std::string ScannedBookKey(const std::filesystem::path &path) {
  std::error_code ec;
  std::filesystem::path canonical = filesystem_compat::WeaklyCanonical(path, ec);
  if (ec) canonical = path;
  canonical = filesystem_compat::LexicallyNormal(canonical);
  return path_adapter::StorePathString(canonical);
}

bool StartNextBootCountRoot(BootRuntimeState &state, const std::vector<std::string> &books_roots) {
  const auto opts = std::filesystem::directory_options::skip_permission_denied;
  while (state.count_root_index < books_roots.size()) {
    const std::filesystem::path root_path(books_roots[state.count_root_index++]);
    std::error_code ec;
    if (!std::filesystem::exists(root_path, ec) || !std::filesystem::is_directory(root_path, ec)) continue;
    state.count_it = std::filesystem::recursive_directory_iterator(root_path, opts, ec);
    if (ec) continue;
    state.count_end = std::filesystem::recursive_directory_iterator();
    state.count_iterator_active = true;
    return true;
  }
  state.count_iterator_active = false;
  return false;
}

float BootProgressRatio(const BootRuntimeState &state) {
  switch (state.phase) {
  case BootPhase::UpdateInstall:
    return state.update_install_done ? 1.0f : std::clamp(0.15f + std::fmod(state.timer * 0.55f, 1.0f) * 0.65f, 0.0f, 0.95f);
  case BootPhase::UpdateReplay:
    return std::clamp(state.timer / kUpdateReplayDurationSec, 0.0f, 1.0f);
  case BootPhase::CountBooks: {
    const float pulse = std::fmod(state.timer * 0.85f, 1.0f);
    return 0.05f + pulse * 0.15f;
  }
  case BootPhase::ScanBooks:
    return state.total_books == 0
               ? 0.55f
               : (0.20f + 0.35f * (static_cast<float>(state.scan_index) /
                                   static_cast<float>(std::max<size_t>(1, state.total_books))));
  case BootPhase::GenerateCovers:
    return state.cover_generate_queue.empty()
               ? 1.0f
               : (0.55f + 0.45f * (static_cast<float>(state.cover_generate_index) /
                                   static_cast<float>(std::max<size_t>(1, state.cover_generate_queue.size()))));
  case BootPhase::Finalize:
  case BootPhase::Done:
    return 1.0f;
  }
  return 0.0f;
}

std::string MakeUpdateInstallText(const BootRuntimeState &state) {
  const float ratio = state.update_install_done ? 0.95f : std::clamp(0.30f + std::fmod(state.timer * 0.40f, 0.55f), 0.30f, 0.85f);
  return LocalizedUpdateReplayText(state.language_index, ratio, true, state.update_replay_version);
}

std::string MakeBootScanText(size_t current, size_t total) {
  return LocalizedBootScanText(0, current, total);
}

std::string MakeBootCoverText(size_t current, size_t total) {
  return LocalizedBootCoverText(0, current, total);
}
std::string MakeUpdateReplayText(float ratio, bool success, const std::string &version) {
  return LocalizedUpdateReplayText(0, ratio, success, version);
}

bool LoadUpdateReplayStatus(const std::filesystem::path &status_path, bool &out_success, std::string &out_version) {
  std::ifstream in(status_path);
  if (!in) return false;
  std::string line;
  bool saw_result = false;
  while (std::getline(in, line)) {
    const size_t eq = line.find('=');
    if (eq == std::string::npos) continue;
    const std::string key = line.substr(0, eq);
    const std::string value = line.substr(eq + 1);
    if (key == "result") {
      out_success = (value == "success");
      saw_result = true;
    } else if (key == "version") {
      out_version = value;
    }
  }
  return saw_result;
}
}

void InitializeBootRuntimeReplay(BootRuntimeState &state, const std::filesystem::path &status_path) {
  bool success = false;
  std::string version;
  if (!LoadUpdateReplayStatus(status_path, success, version)) return;
  state.phase = BootPhase::UpdateReplay;
  state.timer = 0.0f;
  state.update_replay_success = success;
  state.update_replay_version = std::move(version);
  state.update_status_path = status_path;
  state.status_text =
      LocalizedUpdateReplayText(state.language_index, 0.0f, state.update_replay_success, state.update_replay_version);
}

void InitializeBootRuntimePendingUpdate(BootRuntimeState &state) {
  state.phase = BootPhase::UpdateInstall;
  state.timer = 0.0f;
  state.update_install_started = false;
  state.update_install_done = false;
  state.update_install_success = false;
  state.status_text = MakeUpdateInstallText(state);
}

void TickBootRuntime(BootRuntimeState &state, float dt, const BootRuntimeTickDeps &deps) {
  state.timer += dt;
  if (state.phase == BootPhase::UpdateInstall) {
    if (!state.update_install_started) {
      state.update_install_started = true;
      state.status_text = MakeUpdateInstallText(state);
      state.update_install_future = std::async(std::launch::async, [install_pending_update = deps.install_pending_update]() {
        return install_pending_update ? install_pending_update() : false;
      });
    }
    if (!state.update_install_done && state.update_install_future.valid() &&
        state.update_install_future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
      state.update_install_success = state.update_install_future.get();
      state.update_install_done = true;
      state.timer = 0.0f;
    }
    state.status_text = MakeUpdateInstallText(state);
    if (state.update_install_done && state.timer >= 0.8f) {
      state.timer = 0.0f;
      if (state.update_install_success) {
        if (deps.on_update_installed_restart) deps.on_update_installed_restart();
        state.phase = BootPhase::UpdateReplay;
        state.update_replay_success = true;
        state.status_text = LocalizedUpdateReplayText(state.language_index, 0.0f, true, state.update_replay_version);
      } else {
        state.phase = BootPhase::CountBooks;
        state.status_text = LocalizedBootScanText(state.language_index, 0, 0);
      }
    }
  } else if (state.phase == BootPhase::UpdateReplay) {
    const float replay_ratio = std::clamp(state.timer / kUpdateReplayDurationSec, 0.0f, 1.0f);
    state.status_text = LocalizedUpdateReplayText(state.language_index, replay_ratio, state.update_replay_success,
                                                  state.update_replay_version);
    if (replay_ratio >= 1.0f) {
      if (!state.update_status_path.empty()) {
        std::error_code remove_ec;
        std::filesystem::remove(state.update_status_path, remove_ec);
      }
      state.timer = 0.0f;
      state.phase = BootPhase::CountBooks;
      state.status_text = LocalizedBootScanText(state.language_index, 0, 0);
    }
  } else if (state.phase == BootPhase::CountBooks) {
    if (!state.count_iterator_active) {
      StartNextBootCountRoot(state, deps.books_roots);
    }
    size_t processed = 0;
    while (processed < deps.count_batch_entries) {
      if (!state.count_iterator_active) {
        if (!StartNextBootCountRoot(state, deps.books_roots)) break;
      }
      if (state.count_it == state.count_end) {
        state.count_iterator_active = false;
        continue;
      }
      std::error_code ec;
      const auto entry = *state.count_it;
      state.count_it.increment(ec);
      ++processed;
      if (ec || !filesystem_compat::IsRegularFile(entry, ec)) continue;
      const std::string readable_path = path_adapter::ResolveReadableFilePath(entry);
      const std::string ext = deps.get_lower_ext ? deps.get_lower_ext(readable_path) : std::string{};
      if (ext == ".pdf" || ext == ".txt" || ext == ".epub") {
        const std::string book_key = ScannedBookKey(entry.path());
        if (!state.scanned_book_keys.insert(book_key).second) continue;
        BookItem item;
        item.name = entry.path().filename().string();
        item.path = readable_path;
        item.real_path = readable_path;
        item.native_fs_path = entry.path();
        item.is_dir = false;
        state.scanned_books.push_back(std::move(item));
      }
    }
    state.status_text = LocalizedBootScanText(state.language_index, 0, 0);
    if (!state.count_iterator_active && state.count_root_index >= deps.books_roots.size()) {
      std::sort(state.scanned_books.begin(), state.scanned_books.end(),
                [](const BookItem &a, const BookItem &b) {
                  if (a.is_dir != b.is_dir) return a.is_dir > b.is_dir;
                  if (a.path != b.path) return a.path < b.path;
                  return a.name < b.name;
                });
      state.total_books = state.scanned_books.size();
      state.scan_index = 0;
      state.cover_generate_queue.clear();
      state.phase = BootPhase::ScanBooks;
      state.status_text = LocalizedBootScanText(state.language_index, 0, state.total_books);
    }
  } else if (state.phase == BootPhase::ScanBooks) {
    size_t processed = 0;
    while (processed < deps.scan_batch_entries && state.scan_index < state.scanned_books.size()) {
      const std::string &book_path = state.scanned_books[state.scan_index].real_path.empty()
                                         ? state.scanned_books[state.scan_index].path
                                         : state.scanned_books[state.scan_index].real_path;
      const std::string ext = deps.get_lower_ext ? deps.get_lower_ext(book_path) : std::string{};
      if (ext == ".pdf" || ext == ".epub") {
        const BookItem &item = state.scanned_books[state.scan_index];
        const bool manual = deps.has_manual_cover_exact_or_fuzzy && deps.has_manual_cover_exact_or_fuzzy(item);
        const bool cached = deps.has_cached_doc_cover_on_disk && deps.has_cached_doc_cover_on_disk(book_path);
        const bool can_render = deps.doc_cover_backend_available && deps.doc_cover_backend_available(book_path);
        if (!manual && can_render && !cached) {
          state.cover_generate_queue.push_back(book_path);
        }
      }
      ++state.scan_index;
      ++processed;
    }
    state.status_text = LocalizedBootScanText(state.language_index, state.scan_index, state.total_books);
    if (state.scan_index >= state.scanned_books.size()) {
      state.cover_generate_index = 0;
      state.phase = BootPhase::GenerateCovers;
      state.status_text = LocalizedBootCoverText(state.language_index, 0, state.cover_generate_queue.size());
      if (state.cover_generate_queue.empty()) {
        state.phase = BootPhase::Finalize;
      }
    }
  } else if (state.phase == BootPhase::GenerateCovers) {
    size_t processed = 0;
    while (processed < deps.cover_generate_batch_entries &&
           state.cover_generate_index < state.cover_generate_queue.size()) {
      const std::string &doc_path = state.cover_generate_queue[state.cover_generate_index];
      if (deps.create_doc_first_page_cover_texture) {
        if (SDL_Texture *generated = deps.create_doc_first_page_cover_texture(doc_path)) {
          if (deps.destroy_generated_cover_texture) deps.destroy_generated_cover_texture(generated);
        }
      }
      ++state.cover_generate_index;
      ++processed;
    }
    state.status_text =
        LocalizedBootCoverText(state.language_index, state.cover_generate_index, state.cover_generate_queue.size());
    if (state.cover_generate_index >= state.cover_generate_queue.size()) {
      state.phase = BootPhase::Finalize;
    }
  }

  if (state.phase == BootPhase::Finalize) {
    if (deps.on_finish) deps.on_finish(state.total_books, state.cover_generate_queue.size());
    state.phase = BootPhase::Done;
  }
}

void DrawBootRuntime(const BootRuntimeRenderDeps &deps) {
  deps.draw_rect(0, 0, deps.screen_w, deps.screen_h, SDL_Color{20, 20, 24, 255}, true);
  const int bar_x = 40;
  const int bar_y = deps.screen_h / 2;
  const int bar_w = deps.screen_w - 80;
  const float progress = std::clamp(BootProgressRatio(deps.state), 0.0f, 1.0f);
  const int fill_w = static_cast<int>(std::round(progress * bar_w));
  deps.draw_rect(bar_x, bar_y, bar_w, 16, SDL_Color{48, 52, 60, 255}, true);
  deps.draw_rect(bar_x, bar_y, fill_w, 16, SDL_Color{210, 210, 210, 255}, true);
  deps.draw_rect(bar_x, bar_y, bar_w, 16, SDL_Color{255, 255, 255, 220}, false);
#ifdef HAVE_SDL2_TTF
  const SDL_Color boot_text_color{232, 236, 244, 255};
  std::string status_text = deps.state.status_text;
  if (deps.state.phase == BootPhase::UpdateInstall) {
    status_text = MakeUpdateInstallText(deps.state);
  } else if (deps.state.phase == BootPhase::UpdateReplay) {
    const float replay_ratio = std::clamp(deps.state.timer / kUpdateReplayDurationSec, 0.0f, 1.0f);
    status_text = LocalizedUpdateReplayText(deps.language_index, replay_ratio, deps.state.update_replay_success,
                                            deps.state.update_replay_version);
  } else if (deps.state.phase == BootPhase::CountBooks || deps.state.phase == BootPhase::ScanBooks) {
    const size_t current = deps.state.phase == BootPhase::ScanBooks ? deps.state.scan_index : 0;
    status_text = LocalizedBootScanText(deps.language_index, current, deps.state.total_books);
  } else if (deps.state.phase == BootPhase::GenerateCovers || deps.state.phase == BootPhase::Finalize ||
             deps.state.phase == BootPhase::Done) {
    status_text =
        LocalizedBootCoverText(deps.language_index, deps.state.cover_generate_index, deps.state.cover_generate_queue.size());
  }
  if (!status_text.empty() && deps.get_text_texture) {
    if (TextCacheEntry *te = deps.get_text_texture(status_text, boot_text_color); te && te->texture) {
      SDL_Rect td{std::max(0, (deps.screen_w - te->w) / 2), bar_y + 28, te->w, te->h};
      SDL_RenderCopy(deps.renderer, te->texture, nullptr, &td);
    }
  }
#endif
}
