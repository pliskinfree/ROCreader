#include "rgds_drm_runtime.h"

#include <fcntl.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <chrono>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace {

struct Device {
  int fd = -1;
  std::string path;
  std::string name;
};

struct Prompt {
  std::string label;
  int timeout_seconds = 18;
};

const std::vector<Prompt> kPrompts = {
    {"UP"},      {"DOWN"},   {"LEFT"},   {"RIGHT"}, {"A"},      {"B"},
    {"X"},       {"Y"},      {"L1"},     {"R1"},    {"L2"},     {"R2"},
    {"SELECT"},  {"START"},  {"MENU"},   {"RG"},    {"VOLUP"},  {"VOLDOWN"}};

uint8_t GlyphRow(char c, int row) {
  static const char *digits[] = {
      "111101101101111", "010110010010111", "111001111100111", "111001111001111", "101101111001001",
      "111100111001111", "111100111101111", "111001001001001", "111101111101111", "111101111001111"};
  static const char *letters[] = {
      "010101111101101", "110101110101110", "011100100100011", "110101101101110", "111100110100111",
      "111100110100100", "011100101101011", "101101111101101", "111010010010111", "001001001101010",
      "101101110101101", "100100100100111", "101111111101101", "110101101101101", "010101101101010",
      "110101110100100", "010101101111011", "110101110101101", "011100010001110", "111010010010010",
      "101101101101111", "101101101101010", "101101111111101", "101101010101101", "101101010010010",
      "111001010100111"};
  const char *bits = nullptr;
  if (c >= '0' && c <= '9') bits = digits[c - '0'];
  else if (c >= 'A' && c <= 'Z') bits = letters[c - 'A'];
  else return 0;
  uint8_t out = 0;
  for (int col = 0; col < 3; ++col) {
    if (bits[row * 3 + col] == '1') out |= static_cast<uint8_t>(1 << (2 - col));
  }
  return out;
}

void DrawText(rgds::Framebuffer &fb, int x, int y, const std::string &text, int scale, rgds::Rgba color) {
  int cursor = x;
  for (char raw : text) {
    char c = raw >= 'a' && raw <= 'z' ? static_cast<char>(raw - 'a' + 'A') : raw;
    if (c == ' ') {
      cursor += 4 * scale;
      continue;
    }
    for (int row = 0; row < 5; ++row) {
      const uint8_t bits = GlyphRow(c, row);
      for (int col = 0; col < 3; ++col) {
        if ((bits & (1 << (2 - col))) != 0) {
          rgds::DrmRuntime::FillRect(fb, cursor + col * scale, y + row * scale, scale, scale, color);
        }
      }
    }
    cursor += 4 * scale;
  }
}

void DrawPrompt(rgds::DrmRuntime &drm, const Prompt &prompt, int index, int total, int countdown) {
  auto &top = const_cast<rgds::Framebuffer &>(drm.Buffer(rgds::ScreenId::Top));
  auto &bottom = const_cast<rgds::Framebuffer &>(drm.Buffer(rgds::ScreenId::Bottom));
  rgds::DrmRuntime::Clear(top, rgds::Rgba{18, 28, 42, 255});
  rgds::DrmRuntime::Clear(bottom, rgds::Rgba{24, 36, 52, 255});
  DrawText(top, 74, 92, "PRESS", 16, rgds::Rgba{190, 225, 255, 255});
  DrawText(top, 74, 210, prompt.label, 18, rgds::Rgba{255, 255, 255, 255});
  DrawText(bottom, 54, 88, "KEYMAP", 14, rgds::Rgba{190, 225, 255, 255});
  DrawText(bottom, 54, 190, std::to_string(index + 1) + " " + std::to_string(total), 14,
           rgds::Rgba{255, 255, 255, 255});
  DrawText(bottom, 54, 292, std::to_string(countdown), 16, rgds::Rgba{255, 230, 160, 255});
  rgds::DrmRuntime::StrokeRect(top, 8, 8, rgds::kScreenW - 16, rgds::kScreenH - 16, 5,
                               rgds::Rgba{120, 210, 255, 255});
  drm.Present();
}

std::vector<Device> OpenDevices() {
  std::vector<Device> devices;
  for (int i = 0; i < 32; ++i) {
    Device d;
    d.path = "/dev/input/event" + std::to_string(i);
    d.fd = open(d.path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (d.fd < 0) continue;
    char name[128] = {};
    if (ioctl(d.fd, EVIOCGNAME(sizeof(name)), name) >= 0) d.name = name;
    devices.push_back(d);
  }
  return devices;
}

void CloseDevices(std::vector<Device> &devices) {
  for (auto &d : devices) {
    if (d.fd >= 0) close(d.fd);
    d.fd = -1;
  }
}

void DrainDevices(std::vector<Device> &devices, int milliseconds) {
  const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(milliseconds);
  while (std::chrono::steady_clock::now() < end) {
    for (auto &d : devices) {
      input_event ev{};
      while (read(d.fd, &ev, sizeof(ev)) == static_cast<ssize_t>(sizeof(ev))) {
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}

std::string CodeName(int code) {
  switch (code) {
    case KEY_UP: return "KEY_UP";
    case KEY_DOWN: return "KEY_DOWN";
    case KEY_LEFT: return "KEY_LEFT";
    case KEY_RIGHT: return "KEY_RIGHT";
    case KEY_ENTER: return "KEY_ENTER";
    case KEY_SPACE: return "KEY_SPACE";
    case KEY_ESC: return "KEY_ESC";
    case KEY_BACK: return "KEY_BACK";
    case KEY_SELECT: return "KEY_SELECT";
    case KEY_MENU: return "KEY_MENU";
    case KEY_HOMEPAGE: return "KEY_HOMEPAGE";
    case KEY_VOLUMEUP: return "KEY_VOLUMEUP";
    case KEY_VOLUMEDOWN: return "KEY_VOLUMEDOWN";
    case BTN_A: return "BTN_A/BTN_SOUTH";
    case BTN_B: return "BTN_B/BTN_EAST";
    case BTN_X: return "BTN_X/BTN_NORTH";
    case BTN_Y: return "BTN_Y/BTN_WEST";
    case BTN_TL: return "BTN_TL";
    case BTN_TR: return "BTN_TR";
    case BTN_TL2: return "BTN_TL2";
    case BTN_TR2: return "BTN_TR2";
    case BTN_SELECT: return "BTN_SELECT";
    case BTN_START: return "BTN_START";
    case BTN_DPAD_UP: return "BTN_DPAD_UP";
    case BTN_DPAD_DOWN: return "BTN_DPAD_DOWN";
    case BTN_DPAD_LEFT: return "BTN_DPAD_LEFT";
    case BTN_DPAD_RIGHT: return "BTN_DPAD_RIGHT";
    default: return "CODE_" + std::to_string(code);
  }
}

std::string AbsName(int code) {
  switch (code) {
    case ABS_X: return "ABS_X";
    case ABS_Y: return "ABS_Y";
    case ABS_HAT0X: return "ABS_HAT0X";
    case ABS_HAT0Y: return "ABS_HAT0Y";
    default: return "ABS_" + std::to_string(code);
  }
}

}  // namespace

int main() {
  rgds::DrmRuntime drm;
  const rgds::DrmInitResult init = drm.Initialize();
  if (!init.ok) {
    std::cerr << "[keymap] DRM init failed: " << init.error << "\n";
    return 2;
  }
  std::cout << "[keymap] DRM ready: " << drm.DescribeBindings() << "\n";
  std::vector<Device> devices = OpenDevices();
  for (const auto &d : devices) {
    std::cout << "[keymap] device=" << d.path << " name=\"" << d.name << "\"\n";
  }
  for (size_t i = 0; i < kPrompts.size(); ++i) {
    DrainDevices(devices, 350);
    for (int countdown = kPrompts[i].timeout_seconds; countdown > 0; --countdown) {
      DrawPrompt(drm, kPrompts[i], static_cast<int>(i), static_cast<int>(kPrompts.size()), countdown);
      const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(1);
      while (std::chrono::steady_clock::now() < end) {
        bool any_down = false;
        for (auto &d : devices) {
          input_event ev{};
          while (read(d.fd, &ev, sizeof(ev)) == static_cast<ssize_t>(sizeof(ev))) {
            if (ev.type == EV_KEY) {
              std::cout << "[keymap] prompt=" << kPrompts[i].label
                        << " device=" << d.path
                        << " name=\"" << d.name << "\""
                        << " type=" << ev.type
                        << " code=" << ev.code
                        << " code_name=" << CodeName(ev.code)
                        << " value=" << ev.value << "\n";
              if (ev.value == 1) any_down = true;
            } else if (ev.type == EV_ABS) {
              std::cout << "[keymap] prompt=" << kPrompts[i].label
                        << " device=" << d.path
                        << " name=\"" << d.name << "\""
                        << " type=" << ev.type
                        << " abs_code=" << ev.code
                        << " abs_name=" << AbsName(ev.code)
                        << " value=" << ev.value << "\n";
              any_down = true;
            }
          }
        }
        if (any_down) {
          std::this_thread::sleep_for(std::chrono::milliseconds(500));
          countdown = 0;
          break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
    }
    std::cout << "[keymap] prompt=" << kPrompts[i].label << " done\n";
  }
  CloseDevices(devices);
  drm.Shutdown(false);
  return 0;
}
