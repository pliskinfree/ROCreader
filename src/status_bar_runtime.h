#pragma once

#include "input_manager.h"
#include "system_status.h"
#include "ui_text_cache.h"

#include <SDL.h>

#include <functional>

struct StatusBarRenderDeps {
  SDL_Renderer *renderer = nullptr;
  const SystemStatusSnapshot *status = nullptr;
  InputProfile input_profile = InputProfile::DesktopDefault;
  int theme = 0;
  int screen_w = 0;
  int top_bar_y = 0;
  int top_bar_h = 0;
  SDL_Texture *selected_avatar_badge_texture = nullptr;
  std::function<int(int)> scale_px;
  std::function<void(int, int, int, int, SDL_Color, bool)> draw_rect;
  std::function<TextCacheEntry *(const std::string &, SDL_Color)> get_text_texture;
};

void DrawStatusBarRuntime(const StatusBarRenderDeps &deps);

