#include "rgds_drm_runtime.h"
#include "rgds_evdev_input.h"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace {

enum class AppMode {
  Shelf,
  Reader,
  ReaderMenu,
};

enum class Focus {
  Top,
  Bottom,
  Reader,
  ReaderMenu,
};

int ReadEnvInt(const char *name, int fallback_value, int min_value, int max_value) {
  const char *raw = std::getenv(name);
  if (!raw || !*raw) return fallback_value;
  try {
    int value = std::stoi(raw);
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
  } catch (...) {
    return fallback_value;
  }
}

void DrawBoot(rgds::DrmRuntime &drm, int frame) {
  auto &top = drm.DrawBuffer(rgds::ScreenId::Top);
  auto &bottom = drm.DrawBuffer(rgds::ScreenId::Bottom);
  rgds::DrmRuntime::Clear(top, rgds::Rgba{24, 31, 46, 255});
  rgds::DrmRuntime::Clear(bottom, rgds::Rgba{26, 34, 50, 255});
  rgds::DrmRuntime::StrokeRect(top, 70, 206, 500, 22, 2, rgds::Rgba{160, 215, 255, 255});
  rgds::DrmRuntime::FillRect(top, 72, 208, 40 + (frame % 420), 18, rgds::Rgba{95, 175, 238, 255});
  rgds::DrmRuntime::FillRect(bottom, 54, 166, 532, 108, rgds::Rgba{34, 46, 66, 255});
  rgds::DrmRuntime::StrokeRect(bottom, 54, 166, 532, 108, 2, rgds::Rgba{64, 104, 138, 255});
}

void DrawFocusFrame(rgds::Framebuffer &buffer, int pulse) {
  const uint8_t alpha = static_cast<uint8_t>(38 + (pulse % 18));
  rgds::DrmRuntime::BlendRect(buffer, 0, 0, rgds::kScreenW, rgds::kScreenH, rgds::Rgba{90, 190, 255, alpha});
  rgds::DrmRuntime::StrokeRect(buffer, 5, 5, rgds::kScreenW - 10, rgds::kScreenH - 10, 8,
                               rgds::Rgba{120, 210, 255, 255});
}

void DrawShelf(rgds::Framebuffer &top, int selected) {
  rgds::DrmRuntime::Clear(top, rgds::Rgba{26, 27, 31, 255});
  for (int i = 0; i < 8; ++i) {
    const int col = i % 4;
    const int row = i / 4;
    const int x = 42 + col * 145;
    const int y = 68 + row * 190;
    rgds::DrmRuntime::FillRect(top, x, y, 92, 138,
                               rgds::Rgba{static_cast<uint8_t>(58 + i * 13),
                                          static_cast<uint8_t>(84 + i * 8),
                                          static_cast<uint8_t>(112 + i * 6),
                                          255});
    rgds::DrmRuntime::FillRect(top, x + 8, y + 104, 76, 10, rgds::Rgba{230, 235, 238, 255});
    rgds::DrmRuntime::FillRect(top, x + 8, y + 121, 52 + (i % 3) * 8, 8, rgds::Rgba{185, 195, 204, 255});
    if (i == selected) {
      rgds::DrmRuntime::StrokeRect(top, x - 5, y - 5, 102, 148, 4, rgds::Rgba{120, 210, 255, 255});
    }
  }
}

void DrawMenu(rgds::Framebuffer &bottom, int selected) {
  rgds::DrmRuntime::Clear(bottom, rgds::Rgba{22, 32, 42, 255});
  for (int i = 0; i < 6; ++i) {
    const int y = 54 + i * 62;
    const rgds::Rgba fill = (i == selected) ? rgds::Rgba{54, 112, 154, 255} : rgds::Rgba{42, 58, 74, 255};
    rgds::DrmRuntime::FillRect(bottom, 72, y, 496, 42, fill);
    rgds::DrmRuntime::StrokeRect(bottom, 72, y, 496, 42, 1, rgds::Rgba{110, 132, 148, 255});
    rgds::DrmRuntime::FillRect(bottom, 96, y + 15, 260 - i * 22, 8, rgds::Rgba{206, 222, 235, 255});
  }
}

void DrawReader(rgds::Framebuffer &top, rgds::Framebuffer &bottom, int page_offset) {
  rgds::DrmRuntime::Clear(top, rgds::Rgba{238, 235, 225, 255});
  rgds::DrmRuntime::Clear(bottom, rgds::Rgba{232, 229, 219, 255});
  for (int i = 0; i < 26; ++i) {
    const int virtual_y = 34 + i * 36 - (page_offset % 36);
    if (virtual_y < 0 || virtual_y >= rgds::kVirtualReaderH) continue;
    rgds::Framebuffer &target = virtual_y < rgds::kScreenH ? top : bottom;
    const int y = virtual_y < rgds::kScreenH ? virtual_y : virtual_y - rgds::kScreenH;
    const int w = 520 - (i % 6) * 34;
    rgds::DrmRuntime::FillRect(target, 58, y, w, 7, rgds::Rgba{48, 52, 58, 255});
  }
  rgds::DrmRuntime::FillRect(top, 0, rgds::kScreenH - 2, rgds::kScreenW, 2, rgds::Rgba{180, 80, 70, 255});
  rgds::DrmRuntime::FillRect(bottom, 64, rgds::kScreenH - 28, 512, 8, rgds::Rgba{70, 92, 120, 255});
  rgds::DrmRuntime::FillRect(bottom, 64, rgds::kScreenH - 28, 120 + (page_offset % 300), 8,
                             rgds::Rgba{82, 154, 210, 255});
}

void DrawReaderMenu(rgds::Framebuffer &bottom, int selected) {
  rgds::DrmRuntime::BlendRect(bottom, 0, 0, rgds::kScreenW, rgds::kScreenH, rgds::Rgba{14, 18, 24, 210});
  for (int i = 0; i < 4; ++i) {
    const rgds::Rgba fill = (i == selected) ? rgds::Rgba{64, 136, 190, 255} : rgds::Rgba{42, 58, 76, 255};
    rgds::DrmRuntime::FillRect(bottom, 92, 76 + i * 72, 456, 48, fill);
    rgds::DrmRuntime::FillRect(bottom, 120, 95 + i * 72, 270 - i * 32, 9, rgds::Rgba{218, 232, 242, 255});
  }
}

void ApplyAction(const rgds::InputAction &action,
                 AppMode &mode,
                 Focus &focus,
                 int &shelf_selected,
                 int &menu_selected,
                 int &reader_menu_selected,
                 int &page_offset,
                 int &focus_flash,
                 bool &running) {
  if (action.volume_up || action.volume_down) {
    return;
  }
  if (action.exit_app) {
    running = false;
    return;
  }
  if (action.back) {
    if (mode == AppMode::ReaderMenu) {
      mode = AppMode::Reader;
      focus = Focus::Reader;
    } else if (mode == AppMode::Reader) {
      mode = AppMode::Shelf;
      focus = Focus::Top;
    } else {
      running = false;
    }
    focus_flash = 24;
    return;
  }
  if (action.select && mode == AppMode::Shelf) {
    focus = focus == Focus::Top ? Focus::Bottom : Focus::Top;
    focus_flash = 24;
    return;
  }
  if (action.menu) {
    if (mode == AppMode::Reader) {
      mode = AppMode::ReaderMenu;
      focus = Focus::ReaderMenu;
      focus_flash = 24;
    } else if (mode == AppMode::ReaderMenu) {
      mode = AppMode::Reader;
      focus = Focus::Reader;
    }
    return;
  }
  if (action.confirm && mode == AppMode::Shelf && focus == Focus::Top) {
    mode = AppMode::Reader;
    focus = Focus::Reader;
    focus_flash = 24;
    return;
  }
  if (action.right || action.down) {
    if (mode == AppMode::Shelf && focus == Focus::Top) shelf_selected = (shelf_selected + 1) % 8;
    else if (mode == AppMode::Shelf && focus == Focus::Bottom) menu_selected = (menu_selected + 1) % 6;
    else if (mode == AppMode::ReaderMenu) reader_menu_selected = (reader_menu_selected + 1) % 4;
    else page_offset += 36;
    return;
  }
  if (action.left || action.up) {
    if (mode == AppMode::Shelf && focus == Focus::Top) shelf_selected = (shelf_selected + 7) % 8;
    else if (mode == AppMode::Shelf && focus == Focus::Bottom) menu_selected = (menu_selected + 5) % 6;
    else if (mode == AppMode::ReaderMenu) reader_menu_selected = (reader_menu_selected + 3) % 4;
    else page_offset = std::max(0, page_offset - 36);
  }
}

void DrawScene(rgds::DrmRuntime &drm,
               AppMode mode,
               Focus focus,
               int shelf_selected,
               int menu_selected,
               int reader_menu_selected,
               int page_offset,
               int focus_flash) {
  auto &top = drm.DrawBuffer(rgds::ScreenId::Top);
  auto &bottom = drm.DrawBuffer(rgds::ScreenId::Bottom);
  if (mode == AppMode::Shelf) {
    DrawShelf(top, shelf_selected);
    DrawMenu(bottom, menu_selected);
    if (focus_flash > 0) {
      DrawFocusFrame(focus == Focus::Top ? top : bottom, focus_flash);
    }
  } else {
    DrawReader(top, bottom, page_offset);
    if (mode == AppMode::ReaderMenu) {
      DrawReaderMenu(bottom, reader_menu_selected);
      if (focus_flash > 0) DrawFocusFrame(bottom, focus_flash);
    }
  }
}

}  // namespace

int main() {
  const int seconds = ReadEnvInt("ROCREADER_RGDS_READER_SECONDS", 600, 5, 86400);
  rgds::DrmRuntime drm;
  const rgds::DrmInitResult init = drm.Initialize();
  if (!init.ok) {
    std::cerr << "[rgds_reader] DRM init failed: " << init.error << "\n";
    return 2;
  }
  std::cout << "[rgds_reader] DRM ready: " << drm.DescribeBindings() << "\n";

  rgds::EvdevInput input;
  input.OpenAll();
  std::cout << "[rgds_reader] input devices: " << input.DescribeDevices() << "\n";

  AppMode mode = AppMode::Shelf;
  Focus focus = Focus::Top;
  int shelf_selected = 0;
  int menu_selected = 0;
  int reader_menu_selected = 0;
  int page_offset = 0;
  int focus_flash = 24;
  bool running = true;

  DrawBoot(drm, 360);
  drm.Present();
  std::this_thread::sleep_for(std::chrono::milliseconds(700));
  DrawScene(drm, mode, focus, shelf_selected, menu_selected, reader_menu_selected, page_offset, focus_flash);
  drm.Present();

  bool dirty = false;
  for (int frame = 0; running && frame < seconds * 100; ++frame) {
    rgds::InputAction action;
    if (input.Poll(action)) {
      ApplyAction(action, mode, focus, shelf_selected, menu_selected, reader_menu_selected,
                  page_offset, focus_flash, running);
      dirty = true;
    }

    if (dirty) {
      DrawScene(drm, mode, focus, shelf_selected, menu_selected, reader_menu_selected, page_offset, focus_flash);
      drm.Present();
      dirty = false;
    }

    if (focus_flash > 0) {
      --focus_flash;
      if (focus_flash == 0) dirty = true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  input.CloseAll();
  drm.Shutdown(false);
  return 0;
}
