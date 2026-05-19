#include <SDL.h>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

constexpr int kFallbackPanelW = 640;
constexpr int kFallbackPanelH = 480;
constexpr int kFallbackWindowW = kFallbackPanelW * 2;
constexpr int kFallbackWindowH = kFallbackPanelH;

struct Panel {
  SDL_Rect rect{};
  SDL_Color fill{};
  SDL_Color accent{};
  const char *name = "";
};

int ReadEnvInt(const char *name, int fallback_value, int min_value, int max_value) {
  const char *raw = std::getenv(name);
  if (!raw || !*raw) return fallback_value;
  try {
    return std::clamp(std::stoi(raw), min_value, max_value);
  } catch (...) {
    return fallback_value;
  }
}

const char *BoolText(bool value) {
  return value ? "yes" : "no";
}

SDL_Rect ValidBoundsOrFallback(int display_index) {
  SDL_Rect bounds{display_index * kFallbackPanelW, 0, kFallbackPanelW, kFallbackPanelH};
  if (SDL_GetDisplayBounds(display_index, &bounds) != 0 || bounds.w <= 0 || bounds.h <= 0) {
    bounds = SDL_Rect{display_index * kFallbackPanelW, 0, kFallbackPanelW, kFallbackPanelH};
  }
  return bounds;
}

SDL_Rect UnionRect(SDL_Rect a, SDL_Rect b) {
  const int left = std::min(a.x, b.x);
  const int top = std::min(a.y, b.y);
  const int right = std::max(a.x + a.w, b.x + b.w);
  const int bottom = std::max(a.y + a.h, b.y + b.h);
  return SDL_Rect{left, top, right - left, bottom - top};
}

SDL_Rect NormalizeToUnion(SDL_Rect rect, SDL_Rect union_rect) {
  rect.x -= union_rect.x;
  rect.y -= union_rect.y;
  return rect;
}

void DrawBorder(SDL_Renderer *renderer, SDL_Rect rect, SDL_Color color, int thickness) {
  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
  for (int i = 0; i < thickness; ++i) {
    SDL_Rect border{rect.x + i, rect.y + i, std::max(1, rect.w - i * 2), std::max(1, rect.h - i * 2)};
    SDL_RenderDrawRect(renderer, &border);
  }
}

void DrawPanel(SDL_Renderer *renderer, const Panel &panel, int frame) {
  SDL_SetRenderDrawColor(renderer, panel.fill.r, panel.fill.g, panel.fill.b, panel.fill.a);
  SDL_RenderFillRect(renderer, &panel.rect);

  DrawBorder(renderer, panel.rect, panel.accent, 8);

  const int pulse = 32 + ((frame * 5) % 180);
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(renderer, panel.accent.r, panel.accent.g, panel.accent.b, 72);
  SDL_Rect sweep{panel.rect.x + pulse, panel.rect.y + 24, 18, std::max(1, panel.rect.h - 48)};
  if (sweep.x + sweep.w > panel.rect.x + panel.rect.w - 16) {
    sweep.x = panel.rect.x + 16;
  }
  SDL_RenderFillRect(renderer, &sweep);

  SDL_SetRenderDrawColor(renderer, 255, 255, 255, 190);
  const int marker_size = 22;
  for (int i = 0; i < 4; ++i) {
    SDL_Rect marker{panel.rect.x + 28 + i * 42, panel.rect.y + 28, marker_size, marker_size};
    SDL_RenderFillRect(renderer, &marker);
  }

  SDL_SetRenderDrawColor(renderer, 255, 255, 255, 120);
  SDL_RenderDrawLine(renderer, panel.rect.x + 20, panel.rect.y + panel.rect.h - 28,
                     panel.rect.x + panel.rect.w - 20, panel.rect.y + 28);
}

std::vector<Panel> BuildPanels(int displays, SDL_Rect target) {
  const SDL_Color blue{20, 80, 170, 255};
  const SDL_Color cyan{90, 210, 255, 255};
  const SDL_Color red{170, 42, 36, 255};
  const SDL_Color yellow{255, 205, 70, 255};

  if (displays >= 2) {
    SDL_Rect display0 = NormalizeToUnion(ValidBoundsOrFallback(0), target);
    SDL_Rect display1 = NormalizeToUnion(ValidBoundsOrFallback(1), target);
    return std::vector<Panel>{
        Panel{display0, blue, cyan, "display0-blue"},
        Panel{display1, red, yellow, "display1-red"},
    };
  }

  const int half_w = std::max(1, target.w / 2);
  return std::vector<Panel>{
      Panel{SDL_Rect{0, 0, half_w, target.h}, blue, cyan, "left-blue-fallback"},
      Panel{SDL_Rect{half_w, 0, target.w - half_w, target.h}, red, yellow, "right-red-fallback"},
  };
}

}  // namespace

int main(int, char **) {
  const int seconds = ReadEnvInt("ROCREADER_RGDS_SPANNING_SECONDS",
                                 ReadEnvInt("ROCREADER_RGDS_DISPLAY_TEST_SECONDS", 120, 5, 3600),
                                 5, 3600);

  std::cout << "[rgds_spanning_probe] begin seconds=" << seconds << "\n";
  std::cout << "[rgds_spanning_probe] SDL_VIDEODRIVER="
            << (std::getenv("SDL_VIDEODRIVER") ? std::getenv("SDL_VIDEODRIVER") : "") << "\n";
  std::cout << "[rgds_spanning_probe] WAYLAND_DISPLAY="
            << (std::getenv("WAYLAND_DISPLAY") ? std::getenv("WAYLAND_DISPLAY") : "") << "\n";
  std::cout << "[rgds_spanning_probe] XDG_RUNTIME_DIR="
            << (std::getenv("XDG_RUNTIME_DIR") ? std::getenv("XDG_RUNTIME_DIR") : "") << "\n";

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) != 0) {
    std::cout << "[rgds_spanning_probe] SDL_Init failed err=" << SDL_GetError() << "\n";
    return 2;
  }

  std::cout << "[rgds_spanning_probe] current video driver="
            << (SDL_GetCurrentVideoDriver() ? SDL_GetCurrentVideoDriver() : "unknown") << "\n";

  const int displays = SDL_GetNumVideoDisplays();
  std::cout << "[rgds_spanning_probe] displays=" << displays << "\n";
  for (int i = 0; i < displays; ++i) {
    SDL_Rect bounds{};
    SDL_DisplayMode mode{};
    SDL_GetDisplayBounds(i, &bounds);
    SDL_GetCurrentDisplayMode(i, &mode);
    std::cout << "[rgds_spanning_probe] display " << i
              << " name=" << (SDL_GetDisplayName(i) ? SDL_GetDisplayName(i) : "unknown")
              << " bounds=" << bounds.x << "," << bounds.y << " " << bounds.w << "x" << bounds.h
              << " mode=" << mode.w << "x" << mode.h << "@" << mode.refresh_rate << "\n";
  }

  SDL_Rect target{0, 0, kFallbackWindowW, kFallbackWindowH};
  if (displays >= 2) {
    target = UnionRect(ValidBoundsOrFallback(0), ValidBoundsOrFallback(1));
  } else if (displays == 1) {
    target = ValidBoundsOrFallback(0);
  }
  if (target.w < kFallbackWindowW || target.h < kFallbackWindowH) {
    target = SDL_Rect{target.x, target.y, kFallbackWindowW, kFallbackWindowH};
  }

  std::cout << "[rgds_spanning_probe] target_union=" << target.x << "," << target.y
            << " " << target.w << "x" << target.h << "\n";
  std::cout << "[rgds_spanning_probe] window_flags=SDL_WINDOW_SHOWN|SDL_WINDOW_BORDERLESS"
            << " fullscreen=no fullscreen_desktop=no\n";

  SDL_Window *window = SDL_CreateWindow("RGDS Weston spanning probe",
                                        target.x,
                                        target.y,
                                        target.w,
                                        target.h,
                                        SDL_WINDOW_SHOWN | SDL_WINDOW_BORDERLESS);
  if (!window) {
    std::cout << "[rgds_spanning_probe] SDL_CreateWindow failed err=" << SDL_GetError() << "\n";
    SDL_Quit();
    return 3;
  }
  SDL_SetWindowBordered(window, SDL_FALSE);
  SDL_SetWindowPosition(window, target.x, target.y);
  SDL_SetWindowSize(window, target.w, target.h);
  SDL_RaiseWindow(window);

  int actual_w = 0;
  int actual_h = 0;
  int actual_x = 0;
  int actual_y = 0;
  SDL_GetWindowSize(window, &actual_w, &actual_h);
  SDL_GetWindowPosition(window, &actual_x, &actual_y);
  std::cout << "[rgds_spanning_probe] window_actual pos=" << actual_x << "," << actual_y
            << " size=" << actual_w << "x" << actual_h
            << " display_index=" << SDL_GetWindowDisplayIndex(window)
            << " flags=0x" << std::hex << SDL_GetWindowFlags(window) << std::dec << "\n";

  SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!renderer) {
    std::cout << "[rgds_spanning_probe] accelerated renderer failed err=" << SDL_GetError() << "\n";
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
  }
  if (!renderer) {
    std::cout << "[rgds_spanning_probe] SDL_CreateRenderer failed err=" << SDL_GetError() << "\n";
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 4;
  }

  SDL_RendererInfo info{};
  if (SDL_GetRendererInfo(renderer, &info) == 0) {
    std::cout << "[rgds_spanning_probe] renderer name=" << (info.name ? info.name : "unknown")
              << " accelerated=" << BoolText((info.flags & SDL_RENDERER_ACCELERATED) != 0)
              << " target_texture=" << BoolText((info.flags & SDL_RENDERER_TARGETTEXTURE) != 0)
              << " vsync=" << BoolText((info.flags & SDL_RENDERER_PRESENTVSYNC) != 0)
              << " flags=0x" << std::hex << info.flags << std::dec << "\n";
  }

  const std::vector<Panel> panels = BuildPanels(displays, target);
  for (const Panel &panel : panels) {
    std::cout << "[rgds_spanning_probe] panel " << panel.name
              << " rect=" << panel.rect.x << "," << panel.rect.y
              << " " << panel.rect.w << "x" << panel.rect.h << "\n";
  }
  std::cout << "[rgds_spanning_probe] PASS VISUAL: display0/top should stay blue; display1/bottom should stay red\n";

  const Uint32 end_ticks = SDL_GetTicks() + static_cast<Uint32>(seconds * 1000);
  int frame = 0;
  bool running = true;
  while (running && !SDL_TICKS_PASSED(SDL_GetTicks(), end_ticks)) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT) running = false;
      if (event.type == SDL_KEYDOWN && !event.key.repeat) {
        std::cout << "[rgds_spanning_probe] keydown key=" << event.key.keysym.sym
                  << " scancode=" << event.key.keysym.scancode << "\n";
      }
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    for (const Panel &panel : panels) {
      DrawPanel(renderer, panel, frame);
    }
    SDL_RenderPresent(renderer);
    SDL_Delay(16);
    ++frame;
  }

  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  std::cout << "[rgds_spanning_probe] done frames=" << frame << "\n";
  return 0;
}
