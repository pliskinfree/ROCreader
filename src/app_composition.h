#pragma once

#include "app_context.h"
#include "rgds_runtime.h"
#include "screen_profile.h"

struct AppShell;

AppContext MakeAppContext(SDL_Window *window,
                          SDL_Renderer *renderer,
                          rgds::Runtime &rgds_runtime,
                          const ScreenProfile &screen_profile,
                          bool verbose_log);
