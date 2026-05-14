#include "online_shelf_bridge.h"

#include "online_session_service.h"
#include "runtime_log.h"

#include <unordered_map>

namespace {
std::string OnlineCoverCacheKey(const BookItem &item, int cover_w, int cover_h) {
  return "remote|" + item.remote_source_id + "|" + item.remote_id + "|" +
         std::to_string(cover_w) + "x" + std::to_string(cover_h);
}

std::unordered_map<std::string, uint32_t> &CoverRetryAfterTicks() {
  static std::unordered_map<std::string, uint32_t> retry_after;
  return retry_after;
}
}  // namespace

bool OnlineShelfActive(const OnlineSourceState &state) {
  return state.connected;
}

bool RebuildOnlineShelfIfActive(OnlineSourceState &online_source_state, ShelfRuntimeState &shelf_runtime,
                                int nav_selected_index) {
  if (!OnlineShelfActive(online_source_state)) return false;
  const int active_source_index = online_source_state.active_source_index;
  const int category_count =
      active_source_index >= 0 && active_source_index < static_cast<int>(online_source_state.sources.size())
          ? static_cast<int>(online_source_state.sources[active_source_index].categories.size())
          : 0;
  if (nav_selected_index >= 0 && nav_selected_index < category_count &&
      nav_selected_index != online_source_state.loaded_category_index) {
    online_source_state.pending_category_index = nav_selected_index;
    online_source_state.category_loading = true;
    online_source_state.catalog_items.clear();
  }
  shelf_runtime.items = (nav_selected_index == category_count) ? BuildMarkedOnlineShelfItems(online_source_state)
                                                               : BuildOnlineShelfItems(online_source_state);
  runtime_log::Line("online: rebuild shelf nav=" + std::to_string(nav_selected_index) +
                    " items=" + std::to_string(shelf_runtime.items.size()));
  ++shelf_runtime.content_version;
  return true;
}

SDL_Texture *GetOnlineShelfCoverTexture(const BookItem &item, OnlineShelfCoverDeps &deps) {
  if (!item.is_remote) return nullptr;
  const int cover_w = deps.cover_w ? deps.cover_w() : 0;
  const int cover_h = deps.cover_h ? deps.cover_h() : 0;
  const std::string cache_key = OnlineCoverCacheKey(item, cover_w, cover_h);
  if (SDL_Texture *cached = deps.cover_cache.FindTexture(cache_key)) return cached;

  const std::filesystem::path cover_path = OnlineCoverPathForItem(deps.online_source_state, item);
  const bool can_download_now = deps.file_exists && !deps.file_exists(cover_path);
  if (can_download_now) {
    const int active_source_index = deps.online_source_state.active_source_index;
    const bool manual_web = active_source_index >= 0 &&
                            active_source_index < static_cast<int>(deps.online_source_state.sources.size()) &&
                            deps.online_source_state.sources[active_source_index].type == "manual_web";
    if (!manual_web && !deps.online_source_state.covers_loading) {
      const uint32_t now = SDL_GetTicks();
      auto &retry_after = CoverRetryAfterTicks();
      auto retry_it = retry_after.find(cache_key);
      if (retry_it == retry_after.end() || static_cast<int32_t>(now - retry_it->second) >= 0) {
        if (DownloadOnlineCoverForItem(deps.online_source_state, item)) {
          retry_after.erase(cache_key);
        } else {
          retry_after[cache_key] = now + 15000;
        }
      }
    }
  }

  SDL_Texture *texture = nullptr;
  if ((!deps.file_exists || deps.file_exists(cover_path)) && deps.load_surface_from_file) {
    SDL_Surface *surface = deps.load_surface_from_file(cover_path.string());
    if (surface) {
      if (deps.create_normalized_cover_texture) {
        texture = deps.create_normalized_cover_texture(deps.renderer, surface, cover_w, cover_h, deps.cover_aspect);
      }
      if (!texture && deps.create_texture_from_surface) {
        texture = deps.create_texture_from_surface(deps.renderer, surface);
      }
      SDL_FreeSurface(surface);
      if (texture && deps.remember_texture_size) deps.remember_texture_size(texture, cover_w, cover_h);
    }
  }

  if (texture) {
    deps.cover_cache.PutTexture(cache_key, texture, true, deps.get_texture_size,
                                deps.forget_texture_size);
  }
  return texture;
}

SDL_Texture *GetCachedOnlineShelfCoverTexture(const BookItem &item, OnlineShelfCoverDeps &deps) {
  if (!item.is_remote) return nullptr;
  const int cover_w = deps.cover_w ? deps.cover_w() : 0;
  const int cover_h = deps.cover_h ? deps.cover_h() : 0;
  return deps.cover_cache.FindTexture(OnlineCoverCacheKey(item, cover_w, cover_h));
}

bool PreloadOnlineShelfCoverTexture(const BookItem &item, OnlineShelfCoverDeps &deps) {
  return item.is_remote && GetOnlineShelfCoverTexture(item, deps) != nullptr;
}
