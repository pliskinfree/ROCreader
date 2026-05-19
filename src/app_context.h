#pragma once

#include "app_layout.h"
#include "app_config_bridge.h"
#include "screen_profile.h"

#include <SDL.h>

struct AppContext {
  SDL_Window *window = nullptr;
  SDL_Renderer *renderer = nullptr;
  SDL_Window *rgds_bottom_window = nullptr;
  SDL_Renderer *rgds_bottom_renderer = nullptr;
  ScreenProfile screen_profile{};
  const LayoutMetrics *layout = nullptr;
  AppConfigBridge config;
  bool verbose_log = false;
  bool rgds_dual_screen = false;

  int ScreenWidth() const { return layout ? layout->screen_w : 0; }
  int ScreenHeight() const { return layout ? layout->screen_h : 0; }
};
