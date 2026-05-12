#pragma once

#include "book_scanner.h"
#include "cover_cache_runtime.h"
#include "online_source_runtime.h"
#include "shelf_runtime.h"

#include <SDL.h>

#include <functional>
#include <string>

struct OnlineShelfCoverDeps {
  SDL_Renderer *renderer = nullptr;
  OnlineSourceState &online_source_state;
  CoverCacheRuntime &cover_cache;
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
};

bool OnlineShelfActive(const OnlineSourceState &state);
bool RebuildOnlineShelfIfActive(OnlineSourceState &online_source_state, ShelfRuntimeState &shelf_runtime,
                                int nav_selected_index = 0);
SDL_Texture *GetOnlineShelfCoverTexture(const BookItem &item, OnlineShelfCoverDeps &deps);
SDL_Texture *GetCachedOnlineShelfCoverTexture(const BookItem &item, OnlineShelfCoverDeps &deps);
bool PreloadOnlineShelfCoverTexture(const BookItem &item, OnlineShelfCoverDeps &deps);
