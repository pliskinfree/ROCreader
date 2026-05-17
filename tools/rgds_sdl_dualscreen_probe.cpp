#include <SDL.h>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {
int ReadEnvInt(const char *name, int fallback_value, int min_value, int max_value) {
  const char *raw = std::getenv(name);
  if (!raw || !*raw) return fallback_value;
  try {
    return std::clamp(std::stoi(raw), min_value, max_value);
  } catch (...) {
    return fallback_value;
  }
}

const char *BoolText(bool value) { return value ? "yes" : "no"; }

struct Screen {
  SDL_Window *window = nullptr;
  SDL_Renderer *renderer = nullptr;
  int display_index = 0;
  int w = 640;
  int h = 480;
};

void Destroy(Screen &screen) {
  if (screen.renderer) SDL_DestroyRenderer(screen.renderer);
  if (screen.window) SDL_DestroyWindow(screen.window);
  screen.renderer = nullptr;
  screen.window = nullptr;
}

bool CreateScreen(Screen &screen, int display_index, const char *title, int fallback_x) {
  SDL_Rect bounds{fallback_x, 0, 640, 480};
  if (SDL_GetDisplayBounds(display_index, &bounds) != 0) {
    std::cout << "[rgds_probe] SDL_GetDisplayBounds failed display=" << display_index
              << " err=" << SDL_GetError() << "\n";
  }

  SDL_DisplayMode mode{};
  if (SDL_GetCurrentDisplayMode(display_index, &mode) == 0 && mode.w > 0 && mode.h > 0) {
    screen.w = mode.w;
    screen.h = mode.h;
  } else {
    screen.w = bounds.w > 0 ? bounds.w : 640;
    screen.h = bounds.h > 0 ? bounds.h : 480;
  }

  screen.display_index = display_index;
  const int x = bounds.x + 16;
  const int y = bounds.y + 16;
  Uint32 flags = SDL_WINDOW_SHOWN | SDL_WINDOW_FULLSCREEN_DESKTOP;
  screen.window = SDL_CreateWindow(title, x, y, screen.w, screen.h, flags);
  if (!screen.window) {
    std::cout << "[rgds_probe] SDL_CreateWindow failed display=" << display_index
              << " size=" << screen.w << "x" << screen.h
              << " err=" << SDL_GetError() << "\n";
    return false;
  }

  screen.renderer = SDL_CreateRenderer(screen.window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!screen.renderer) {
    std::cout << "[rgds_probe] accelerated renderer failed display=" << display_index
              << " err=" << SDL_GetError() << "\n";
    screen.renderer = SDL_CreateRenderer(screen.window, -1, SDL_RENDERER_SOFTWARE);
  }
  if (!screen.renderer) {
    std::cout << "[rgds_probe] SDL_CreateRenderer failed display=" << display_index
              << " err=" << SDL_GetError() << "\n";
    return false;
  }

  SDL_RendererInfo info{};
  if (SDL_GetRendererInfo(screen.renderer, &info) == 0) {
    std::cout << "[rgds_probe] renderer display=" << display_index
              << " name=" << (info.name ? info.name : "unknown")
              << " accelerated=" << BoolText((info.flags & SDL_RENDERER_ACCELERATED) != 0)
              << " target_texture=" << BoolText((info.flags & SDL_RENDERER_TARGETTEXTURE) != 0)
              << " vsync=" << BoolText((info.flags & SDL_RENDERER_PRESENTVSYNC) != 0)
              << "\n";
  }
  std::cout << "[rgds_probe] created display=" << display_index
            << " bounds=" << bounds.x << "," << bounds.y << " " << bounds.w << "x" << bounds.h
            << " window_size=" << screen.w << "x" << screen.h << "\n";
  return true;
}

void DrawScreen(Screen &screen, SDL_Color background, bool focused, int frame_index) {
  SDL_SetRenderDrawColor(screen.renderer, background.r, background.g, background.b, 255);
  SDL_RenderClear(screen.renderer);

  const int pulse = (frame_index / 20) % 2 == 0 ? 190 : 90;
  if (focused) {
    SDL_SetRenderDrawBlendMode(screen.renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(screen.renderer, 90, 190, 255, 42);
    SDL_Rect fill{0, 0, screen.w, screen.h};
    SDL_RenderFillRect(screen.renderer, &fill);
    SDL_SetRenderDrawColor(screen.renderer, 120, 210, 255, static_cast<Uint8>(pulse));
    for (int i = 0; i < 8; ++i) {
      SDL_Rect border{i, i, std::max(1, screen.w - i * 2), std::max(1, screen.h - i * 2)};
      SDL_RenderDrawRect(screen.renderer, &border);
    }
  }

  SDL_RenderPresent(screen.renderer);
}
}  // namespace

int main(int, char **) {
  const int seconds = ReadEnvInt("ROCREADER_RGDS_DISPLAY_TEST_SECONDS", 15, 1, 300);
  std::cout << "[rgds_probe] begin seconds=" << seconds << "\n";
  std::cout << "[rgds_probe] SDL_VIDEODRIVER=" << (std::getenv("SDL_VIDEODRIVER") ? std::getenv("SDL_VIDEODRIVER") : "")
            << "\n";

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) != 0) {
    std::cout << "[rgds_probe] SDL_Init failed err=" << SDL_GetError() << "\n";
    return 2;
  }

  std::cout << "[rgds_probe] current video driver=" << (SDL_GetCurrentVideoDriver() ? SDL_GetCurrentVideoDriver() : "unknown")
            << "\n";
  const int displays = SDL_GetNumVideoDisplays();
  std::cout << "[rgds_probe] displays=" << displays << "\n";
  for (int i = 0; i < displays; ++i) {
    SDL_Rect bounds{};
    SDL_DisplayMode mode{};
    SDL_GetDisplayBounds(i, &bounds);
    SDL_GetCurrentDisplayMode(i, &mode);
    std::cout << "[rgds_probe] display " << i
              << " name=" << (SDL_GetDisplayName(i) ? SDL_GetDisplayName(i) : "unknown")
              << " bounds=" << bounds.x << "," << bounds.y << " " << bounds.w << "x" << bounds.h
              << " mode=" << mode.w << "x" << mode.h << "@" << mode.refresh_rate << "\n";
  }

  if (displays < 2) {
    std::cout << "[rgds_probe] only one SDL display exposed; true dual-window RGDS path is not viable through SDL yet\n";
  }

  std::vector<Screen> screens;
  screens.resize(std::min(2, std::max(1, displays)));
  if (!CreateScreen(screens[0], 0, "RGDS top probe", 0)) {
    Destroy(screens[0]);
    SDL_Quit();
    return 3;
  }
  if (displays >= 2) {
    if (!CreateScreen(screens[1], 1, "RGDS bottom probe", 640)) {
      Destroy(screens[1]);
      Destroy(screens[0]);
      SDL_Quit();
      return 4;
    }
  }

  const Uint32 end_ticks = SDL_GetTicks() + static_cast<Uint32>(seconds * 1000);
  int frame = 0;
  bool running = true;
  while (running && !SDL_TICKS_PASSED(SDL_GetTicks(), end_ticks)) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT) running = false;
      if (event.type == SDL_KEYDOWN && !event.key.repeat) {
        std::cout << "[rgds_probe] keydown key=" << event.key.keysym.sym
                  << " scancode=" << event.key.keysym.scancode << "\n";
      }
      if (event.type == SDL_JOYBUTTONDOWN) {
        std::cout << "[rgds_probe] joybutton button=" << static_cast<int>(event.jbutton.button) << "\n";
      }
      if (event.type == SDL_CONTROLLERBUTTONDOWN) {
        std::cout << "[rgds_probe] controllerbutton button=" << static_cast<int>(event.cbutton.button) << "\n";
      }
    }
    const bool focus_top = (frame / 60) % 2 == 0;
    DrawScreen(screens[0], SDL_Color{24, 74, 142, 255}, focus_top, frame);
    if (screens.size() > 1 && screens[1].renderer) {
      DrawScreen(screens[1], SDL_Color{32, 114, 78, 255}, !focus_top, frame);
    }
    SDL_Delay(16);
    ++frame;
  }

  for (auto &screen : screens) Destroy(screen);
  SDL_Quit();
  std::cout << "[rgds_probe] done frames=" << frame << "\n";
  return 0;
}
