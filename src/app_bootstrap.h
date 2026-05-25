#pragma once

#include "rgds_runtime.h"
#include "screen_profile.h"

#include <SDL.h>

#include <string>

using FatalSignalHandlerFn = void (*)(int);

struct AppBootstrapResult {
  bool ok = false;
  int exit_code = 0;
  SDL_Window *window = nullptr;
  SDL_Renderer *renderer = nullptr;
  ScreenProfile screen_profile;
  std::string device_model_token;
  rgds::Runtime rgds_runtime;
  bool verbose_log = false;
  bool renderer_supports_target_textures = false;
};

void InitializeProcessRuntime(const char *argv0, FatalSignalHandlerFn fatal_signal_handler);
AppBootstrapResult BootstrapSdlApp();
