#include "app_composition.h"

#include "app_layout.h"

AppContext MakeAppContext(SDL_Window *window,
                          SDL_Renderer *renderer,
                          rgds::Runtime &rgds_runtime,
                          const ScreenProfile &screen_profile,
                          bool verbose_log) {
  AppContext context;
  context.window = window;
  context.renderer = renderer;
  context.rgds_bottom_window = rgds_runtime.bottom_window;
  context.rgds_bottom_renderer = rgds_runtime.bottom_renderer;
  context.rgds_dual_screen = rgds::IsActive(rgds_runtime);
  context.screen_profile = screen_profile;
  context.layout = &Layout();
  context.verbose_log = verbose_log;
  return context;
}
