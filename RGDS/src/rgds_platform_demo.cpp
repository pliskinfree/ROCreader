#include "rgds_dual_screen_runtime.h"

#include <SDL.h>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

enum class DemoMode {
  ShelfMenu,
  Reader,
  ReaderMenu,
};

enum class Focus {
  TopShelf,
  BottomMenu,
  ReaderContent,
  ReaderMenu,
};

struct InputAction {
  bool select = false;
  bool menu = false;
  bool back = false;
  bool confirm = false;
  bool up = false;
  bool down = false;
  bool left = false;
  bool right = false;
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

bool IsSelectKey(SDL_Keycode key, SDL_Scancode scancode) {
  return key == SDLK_SELECT || key == SDLK_TAB || scancode == SDL_SCANCODE_SELECT;
}

bool IsMenuKey(SDL_Keycode key, SDL_Scancode scancode) {
  return key == SDLK_MENU || key == SDLK_m || scancode == SDL_SCANCODE_MENU;
}

bool IsBackKey(SDL_Keycode key, SDL_Scancode scancode) {
  return key == SDLK_AC_BACK || key == SDLK_ESCAPE || scancode == SDL_SCANCODE_AC_BACK ||
         scancode == SDL_SCANCODE_BACKSPACE;
}

bool IsConfirmKey(SDL_Keycode key, SDL_Scancode scancode) {
  return key == SDLK_RETURN || key == SDLK_KP_ENTER || key == SDLK_SPACE ||
         scancode == SDL_SCANCODE_A || scancode == SDL_SCANCODE_RETURN;
}

bool IsUpKey(SDL_Keycode key, SDL_Scancode scancode) {
  return key == SDLK_UP || scancode == SDL_SCANCODE_UP;
}

bool IsDownKey(SDL_Keycode key, SDL_Scancode scancode) {
  return key == SDLK_DOWN || scancode == SDL_SCANCODE_DOWN;
}

bool IsLeftKey(SDL_Keycode key, SDL_Scancode scancode) {
  return key == SDLK_LEFT || scancode == SDL_SCANCODE_LEFT;
}

bool IsRightKey(SDL_Keycode key, SDL_Scancode scancode) {
  return key == SDLK_RIGHT || scancode == SDL_SCANCODE_RIGHT;
}

InputAction ActionFromKey(const SDL_KeyboardEvent &event) {
  InputAction action;
  const SDL_Keycode key = event.keysym.sym;
  const SDL_Scancode scancode = event.keysym.scancode;
  action.select = IsSelectKey(key, scancode);
  action.menu = IsMenuKey(key, scancode);
  action.back = IsBackKey(key, scancode);
  action.confirm = IsConfirmKey(key, scancode);
  action.up = IsUpKey(key, scancode);
  action.down = IsDownKey(key, scancode);
  action.left = IsLeftKey(key, scancode);
  action.right = IsRightKey(key, scancode);
  return action;
}

InputAction ActionFromControllerButton(Uint8 button) {
  InputAction action;
  switch (button) {
    case SDL_CONTROLLER_BUTTON_DPAD_UP:
      action.up = true;
      break;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
      action.down = true;
      break;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
      action.left = true;
      break;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
      action.right = true;
      break;
    case SDL_CONTROLLER_BUTTON_A:
      action.confirm = true;
      break;
    case SDL_CONTROLLER_BUTTON_B:
      action.back = true;
      break;
    case SDL_CONTROLLER_BUTTON_BACK:
      action.select = true;
      break;
    case SDL_CONTROLLER_BUTTON_START:
    case SDL_CONTROLLER_BUTTON_GUIDE:
      action.menu = true;
      break;
    default:
      break;
  }
  return action;
}

InputAction ActionFromJoystickButton(Uint8 button) {
  InputAction action;
  switch (button) {
    case 0:
      action.confirm = true;
      break;
    case 1:
      action.back = true;
      break;
    case 6:
      action.select = true;
      break;
    case 7:
      action.menu = true;
      break;
    default:
      break;
  }
  return action;
}

InputAction ActionFromHat(Uint8 value) {
  InputAction action;
  action.up = (value & SDL_HAT_UP) != 0;
  action.down = (value & SDL_HAT_DOWN) != 0;
  action.left = (value & SDL_HAT_LEFT) != 0;
  action.right = (value & SDL_HAT_RIGHT) != 0;
  return action;
}

void DrawMockShelf(SDL_Renderer *renderer, int selected) {
  SDL_SetRenderDrawColor(renderer, 26, 27, 31, 255);
  SDL_RenderClear(renderer);
  for (int i = 0; i < 8; ++i) {
    const int col = i % 4;
    const int row = i / 4;
    SDL_Rect card{42 + col * 145, 68 + row * 190, 92, 138};
    SDL_SetRenderDrawColor(renderer, 60 + i * 12, 88 + i * 9, 118 + i * 7, 255);
    SDL_RenderFillRect(renderer, &card);
    if (i == selected) {
      SDL_SetRenderDrawColor(renderer, 120, 210, 255, 255);
      for (int b = 0; b < 4; ++b) {
        SDL_Rect border{card.x - b - 3, card.y - b - 3, card.w + (b + 3) * 2, card.h + (b + 3) * 2};
        SDL_RenderDrawRect(renderer, &border);
      }
    }
  }
}

void DrawMockMenu(SDL_Renderer *renderer, int selected) {
  SDL_SetRenderDrawColor(renderer, 22, 32, 42, 255);
  SDL_RenderClear(renderer);
  for (int i = 0; i < 6; ++i) {
    SDL_Rect row{72, 54 + i * 62, 496, 42};
    if (i == selected) SDL_SetRenderDrawColor(renderer, 54, 112, 154, 255);
    else SDL_SetRenderDrawColor(renderer, 42, 58, 74, 255);
    SDL_RenderFillRect(renderer, &row);
    SDL_SetRenderDrawColor(renderer, 110, 132, 148, 255);
    SDL_RenderDrawRect(renderer, &row);
  }
}

void DrawReaderCanvas(SDL_Renderer *renderer, int page_offset) {
  SDL_SetRenderDrawColor(renderer, 238, 235, 225, 255);
  SDL_RenderClear(renderer);
  for (int y = 0; y < rgds::kVirtualReaderH; y += 40) {
    const Uint8 shade = static_cast<Uint8>(220 - (y / 40) % 2 * 12);
    SDL_SetRenderDrawColor(renderer, shade, shade, static_cast<Uint8>(shade - 8), 255);
    SDL_Rect band{0, y, rgds::kVirtualReaderW, 40};
    SDL_RenderFillRect(renderer, &band);
  }
  SDL_SetRenderDrawColor(renderer, 48, 52, 58, 255);
  for (int i = 0; i < 22; ++i) {
    const int y = 36 + i * 39 - (page_offset % 39);
    if (y < 0 || y >= rgds::kVirtualReaderH) continue;
    SDL_Rect line{58, y, 524 - (i % 5) * 42, 7};
    SDL_RenderFillRect(renderer, &line);
  }
  SDL_SetRenderDrawColor(renderer, 180, 80, 70, 255);
  SDL_Rect split_hint{0, rgds::kScreenH - 1, rgds::kVirtualReaderW, 2};
  SDL_RenderFillRect(renderer, &split_hint);
}

void DrawReaderMenuOverlay(SDL_Renderer *renderer, int selected) {
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(renderer, 14, 18, 24, 210);
  SDL_Rect panel{0, 0, rgds::kScreenW, rgds::kScreenH};
  SDL_RenderFillRect(renderer, &panel);
  for (int i = 0; i < 4; ++i) {
    SDL_Rect item{92, 76 + i * 72, 456, 48};
    if (i == selected) SDL_SetRenderDrawColor(renderer, 64, 136, 190, 255);
    else SDL_SetRenderDrawColor(renderer, 42, 58, 76, 255);
    SDL_RenderFillRect(renderer, &item);
  }
}

void DrawBootTop(SDL_Renderer *top_renderer, int tick, bool focused) {
  SDL_SetRenderDrawColor(top_renderer, 24, 31, 46, 255);
  SDL_RenderClear(top_renderer);

  const int bar_width = 420;
  const int bar_height = 18;
  const int progress = 40 + (tick % 220);
  SDL_Rect outline_top{(rgds::kScreenW - bar_width) / 2, 200, bar_width, bar_height};
  SDL_Rect fill_top{outline_top.x, outline_top.y, std::min(bar_width, progress), bar_height};
  SDL_SetRenderDrawColor(top_renderer, 100, 178, 240, 255);
  SDL_RenderFillRect(top_renderer, &fill_top);
  SDL_SetRenderDrawColor(top_renderer, 160, 215, 255, 255);
  SDL_RenderDrawRect(top_renderer, &outline_top);

  if (focused) {
    SDL_SetRenderDrawColor(top_renderer, 120, 210, 255, 40);
    SDL_Rect fill{0, 0, rgds::kScreenW, rgds::kScreenH};
    SDL_RenderFillRect(top_renderer, &fill);
  }
}

void DrawBootBottom(SDL_Renderer *bottom_renderer, bool focused) {
  SDL_SetRenderDrawColor(bottom_renderer, 26, 34, 50, 255);
  SDL_RenderClear(bottom_renderer);

  SDL_Rect bottom_panel{54, 166, 532, 108};
  SDL_SetRenderDrawColor(bottom_renderer, 34, 46, 66, 255);
  SDL_RenderFillRect(bottom_renderer, &bottom_panel);
  SDL_SetRenderDrawColor(bottom_renderer, 64, 104, 138, 255);
  SDL_RenderDrawRect(bottom_renderer, &bottom_panel);

  if (focused) {
    SDL_SetRenderDrawColor(bottom_renderer, 120, 210, 255, 40);
    SDL_Rect fill{0, 0, rgds::kScreenW, rgds::kScreenH};
    SDL_RenderFillRect(bottom_renderer, &fill);
  }
}

void ApplyAction(const InputAction &action,
                 DemoMode &mode,
                 Focus &focus,
                 uint32_t &focus_flash_until,
                 int &shelf_selected,
                 int &menu_selected,
                 int &reader_menu_selected,
                 int &page_offset,
                 bool &running) {
  if (action.back) {
    if (mode == DemoMode::Reader || mode == DemoMode::ReaderMenu) {
      mode = DemoMode::ShelfMenu;
      focus = Focus::TopShelf;
      focus_flash_until = SDL_GetTicks() + 450;
    } else {
      running = false;
    }
    return;
  }

  if (action.select) {
    if (mode == DemoMode::ShelfMenu) {
      focus = (focus == Focus::TopShelf) ? Focus::BottomMenu : Focus::TopShelf;
      focus_flash_until = SDL_GetTicks() + 450;
    }
    return;
  }

  if (action.confirm) {
    if (mode == DemoMode::ShelfMenu && focus == Focus::TopShelf) {
      mode = DemoMode::Reader;
      focus = Focus::ReaderContent;
      focus_flash_until = SDL_GetTicks() + 450;
    }
    return;
  }

  if (action.menu) {
    if (mode == DemoMode::Reader) {
      mode = DemoMode::ReaderMenu;
      focus = Focus::ReaderMenu;
      focus_flash_until = SDL_GetTicks() + 450;
    } else if (mode == DemoMode::ReaderMenu) {
      mode = DemoMode::Reader;
      focus = Focus::ReaderContent;
      focus_flash_until = SDL_GetTicks() + 450;
    }
    return;
  }

  if (action.right || action.down) {
    if (mode == DemoMode::ShelfMenu && focus == Focus::TopShelf) shelf_selected = (shelf_selected + 1) % 8;
    else if (mode == DemoMode::ShelfMenu && focus == Focus::BottomMenu) menu_selected = (menu_selected + 1) % 6;
    else if (mode == DemoMode::ReaderMenu) reader_menu_selected = (reader_menu_selected + 1) % 4;
    else page_offset += 40;
    return;
  }

  if (action.left || action.up) {
    if (mode == DemoMode::ShelfMenu && focus == Focus::TopShelf) shelf_selected = (shelf_selected + 7) % 8;
    else if (mode == DemoMode::ShelfMenu && focus == Focus::BottomMenu) menu_selected = (menu_selected + 5) % 6;
    else if (mode == DemoMode::ReaderMenu) reader_menu_selected = (reader_menu_selected + 3) % 4;
    else page_offset = std::max(0, page_offset - 40);
  }
}

}  // namespace

int main(int, char **) {
  if (std::getenv("SDL_VIDEODRIVER") == nullptr) {
    SDL_setenv("SDL_VIDEODRIVER", "KMSDRM", 0);
  }
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) != 0) {
    std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
    return 2;
  }

  rgds::DualScreenRuntime dual;
  const rgds::DualScreenInitResult init = dual.Initialize("ROCreader RGDS Top", "ROCreader RGDS Bottom");
  if (!init.ok) {
    std::cerr << "Dual screen init failed: " << init.error << "\n";
    SDL_Quit();
    return 3;
  }
  if (!dual.EnsureReaderCanvas()) {
    std::cerr << "Reader canvas create failed: " << SDL_GetError() << "\n";
    SDL_Quit();
    return 4;
  }

  const int seconds = ReadEnvInt("ROCREADER_RGDS_DEMO_SECONDS", 60, 5, 3600);
  std::cout << "[rgds_demo] current video driver="
            << (SDL_GetCurrentVideoDriver() ? SDL_GetCurrentVideoDriver() : "unknown") << "\n";
  const int display_count = SDL_GetNumVideoDisplays();
  std::cout << "[rgds_demo] displays=" << display_count << "\n";
  for (int i = 0; i < display_count; ++i) {
    SDL_Rect bounds{};
    SDL_DisplayMode mode{};
    SDL_GetDisplayBounds(i, &bounds);
    SDL_GetCurrentDisplayMode(i, &mode);
    std::cout << "[rgds_demo] display " << i
              << " bounds=" << bounds.x << "," << bounds.y << " " << bounds.w << "x" << bounds.h
              << " mode=" << mode.w << "x" << mode.h << "@" << mode.refresh_rate << "\n";
  }

  const uint32_t end_ticks = SDL_GetTicks() + static_cast<uint32_t>(seconds * 1000);
  DemoMode mode = DemoMode::ShelfMenu;
  Focus focus = Focus::TopShelf;
  uint32_t focus_flash_until = SDL_GetTicks() + 800;
  int shelf_selected = 0;
  int menu_selected = 0;
  int reader_menu_selected = 0;
  int page_offset = 0;
  bool boot_phase = true;
  bool running = true;

  const int joysticks = SDL_NumJoysticks();
  std::cout << "[rgds_demo] joysticks=" << joysticks << "\n";
  for (int i = 0; i < joysticks; ++i) {
    if (SDL_IsGameController(i)) {
      std::cout << "[rgds_demo] opening controller index=" << i << "\n";
      SDL_GameControllerOpen(i);
    } else {
      std::cout << "[rgds_demo] opening joystick index=" << i << "\n";
      SDL_JoystickOpen(i);
    }
  }

  while (running && !SDL_TICKS_PASSED(SDL_GetTicks(), end_ticks)) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT) running = false;
      if (event.type == SDL_KEYDOWN && !event.key.repeat) {
        std::cout << "[rgds_demo] keydown key=" << event.key.keysym.sym
                  << " scancode=" << event.key.keysym.scancode << "\n";
        const InputAction action = ActionFromKey(event.key);
        ApplyAction(action, mode, focus, focus_flash_until, shelf_selected, menu_selected,
                    reader_menu_selected, page_offset, running);
      } else if (event.type == SDL_CONTROLLERBUTTONDOWN) {
        std::cout << "[rgds_demo] controllerbutton button=" << static_cast<int>(event.cbutton.button) << "\n";
        const InputAction action = ActionFromControllerButton(event.cbutton.button);
        ApplyAction(action, mode, focus, focus_flash_until, shelf_selected, menu_selected,
                    reader_menu_selected, page_offset, running);
      } else if (event.type == SDL_JOYBUTTONDOWN) {
        std::cout << "[rgds_demo] joybutton button=" << static_cast<int>(event.jbutton.button) << "\n";
        const InputAction action = ActionFromJoystickButton(event.jbutton.button);
        ApplyAction(action, mode, focus, focus_flash_until, shelf_selected, menu_selected,
                    reader_menu_selected, page_offset, running);
      } else if (event.type == SDL_JOYHATMOTION && event.jhat.value != SDL_HAT_CENTERED) {
        std::cout << "[rgds_demo] joyhat value=" << static_cast<int>(event.jhat.value) << "\n";
        const InputAction action = ActionFromHat(event.jhat.value);
        ApplyAction(action, mode, focus, focus_flash_until, shelf_selected, menu_selected,
                    reader_menu_selected, page_offset, running);
      }
    }

    if (boot_phase) {
      SDL_Renderer *top_renderer = dual.BeginSurface(rgds::SurfaceId::Top);
      DrawBootTop(top_renderer, static_cast<int>(SDL_GetTicks() / 16), focus == Focus::TopShelf);
      dual.EndSurface();
      SDL_Renderer *bottom_renderer = dual.BeginSurface(rgds::SurfaceId::Bottom);
      DrawBootBottom(bottom_renderer, focus != Focus::TopShelf);
      dual.EndSurface();
      if (SDL_GetTicks() > focus_flash_until) {
        boot_phase = false;
        focus_flash_until = SDL_GetTicks() + 450;
      }
    } else if (mode == DemoMode::ShelfMenu) {
      SDL_Renderer *top_renderer = dual.BeginSurface(rgds::SurfaceId::Top);
      DrawMockShelf(top_renderer, shelf_selected);
      dual.EndSurface();
      SDL_Renderer *bottom_renderer = dual.BeginSurface(rgds::SurfaceId::Bottom);
      DrawMockMenu(bottom_renderer, menu_selected);
      dual.EndSurface();
      const float flash = SDL_TICKS_PASSED(focus_flash_until, SDL_GetTicks())
                              ? 0.0f
                              : static_cast<float>(focus_flash_until - SDL_GetTicks()) / 450.0f;
      if (focus == Focus::TopShelf) dual.DrawFocusFrame(rgds::SurfaceId::Top, flash);
      else dual.DrawFocusFrame(rgds::SurfaceId::Bottom, flash);
    } else {
      SDL_Renderer *top_renderer = dual.BeginSurface(rgds::SurfaceId::Top);
      SDL_SetRenderTarget(top_renderer, dual.ReaderCanvas());
      DrawReaderCanvas(top_renderer, page_offset);
      SDL_SetRenderTarget(top_renderer, nullptr);
      dual.EndSurface();
      dual.ClearSurface(rgds::SurfaceId::Top, SDL_Color{0, 0, 0, 255});
      dual.ClearSurface(rgds::SurfaceId::Bottom, SDL_Color{0, 0, 0, 255});
      dual.PresentReaderCanvasSplit();
      if (mode == DemoMode::ReaderMenu) {
        SDL_Renderer *bottom_renderer = dual.BeginSurface(rgds::SurfaceId::Bottom);
        DrawReaderMenuOverlay(bottom_renderer, reader_menu_selected);
        dual.EndSurface();
      }
      const float flash = SDL_TICKS_PASSED(focus_flash_until, SDL_GetTicks())
                              ? 0.0f
                              : static_cast<float>(focus_flash_until - SDL_GetTicks()) / 450.0f;
      if (mode == DemoMode::ReaderMenu) dual.DrawFocusFrame(rgds::SurfaceId::Bottom, flash);
    }
    dual.PresentBoth();
    SDL_Delay(16);
  }

  dual.Shutdown();
  SDL_Quit();
  return 0;
}
