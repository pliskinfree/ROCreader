#pragma once

#include "filesystem_compat.h"
#include "input_manager.h"
#include "screen_profile.h"

#include <SDL.h>

struct AppRenderEnv {
  SDL_Window *window = nullptr;
  SDL_Renderer *renderer = nullptr;
  const ScreenProfile *screen_profile = nullptr;
  int screen_w = 0;
  int screen_h = 0;
};

enum class PlatformLayoutMode {
  SingleScreen,
  RgdsWestonSpanning,
  RgdsStackedPreview,
};

struct PlatformCapabilities {
  InputProfile input_profile = InputProfile::DesktopDefault;
  PlatformLayoutMode layout_mode = PlatformLayoutMode::SingleScreen;
  bool use_h700_defaults = false;
  bool use_trimui_brick_keymap = false;
  bool has_dual_screen = false;
  bool has_system_volume = false;
  bool has_lid_control = false;
};

struct AppPlatformEnv {
  PlatformCapabilities capabilities;
  bool rgds_dual_screen = false;
};

struct AppStorageEnv {
  std::filesystem::path runtime_root;
  std::filesystem::path books_root;
  std::filesystem::path cover_root;
  std::filesystem::path cache_root;
  std::filesystem::path downloads_root;
};
