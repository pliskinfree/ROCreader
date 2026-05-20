#include "rgds_drm_runtime.h"

#include <fcntl.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cerrno>
#include <cmath>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

struct AxisState {
  int code = 0;
  int minimum = 0;
  int maximum = 0;
  int flat = 0;
  int current = 0;
  bool valid = false;
};

struct Device {
  int fd = -1;
  std::string path;
  std::string name;
  std::vector<AxisState> axes;
};

struct Prompt {
  std::string key;
  std::string verb;
  std::string line1;
  std::string line2;
  bool click = false;
  int timeout_seconds = 20;
};

const std::vector<int> kAxisCodes = {
    ABS_X, ABS_Y, ABS_Z, ABS_RX, ABS_RY, ABS_RZ,
    ABS_HAT0X, ABS_HAT0Y, ABS_HAT1X, ABS_HAT1Y,
    ABS_HAT2X, ABS_HAT2Y, ABS_HAT3X, ABS_HAT3Y};

const std::vector<Prompt> kPrompts = {
    {"LEFT_STICK_UP", "MOVE", "LEFT STICK", "UP"},
    {"LEFT_STICK_DOWN", "MOVE", "LEFT STICK", "DOWN"},
    {"LEFT_STICK_LEFT", "MOVE", "LEFT STICK", "LEFT"},
    {"LEFT_STICK_RIGHT", "MOVE", "LEFT STICK", "RIGHT"},
    {"LEFT_STICK_CLICK", "PRESS", "LEFT STICK", "CLICK", true},
    {"RIGHT_STICK_UP", "MOVE", "RIGHT STICK", "UP"},
    {"RIGHT_STICK_DOWN", "MOVE", "RIGHT STICK", "DOWN"},
    {"RIGHT_STICK_LEFT", "MOVE", "RIGHT STICK", "LEFT"},
    {"RIGHT_STICK_RIGHT", "MOVE", "RIGHT STICK", "RIGHT"},
    {"RIGHT_STICK_CLICK", "PRESS", "RIGHT STICK", "CLICK", true},
};

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

int TextWidth(const std::string &text, int scale) {
  int width = 0;
  for (char raw : text) {
    const char c = raw >= 'a' && raw <= 'z' ? static_cast<char>(raw - 'a' + 'A') : raw;
    width += c == ' ' ? 4 * scale : 4 * scale;
  }
  return std::max(0, width - scale);
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

void DrawCentered(rgds::Framebuffer &fb, int y, const std::string &text, int scale, rgds::Rgba color) {
  DrawText(fb, (rgds::kScreenW - TextWidth(text, scale)) / 2, y, text, scale, color);
}

void DrawPrompt(rgds::DrmRuntime &drm, const Prompt &prompt, int index, int total, int countdown) {
  auto &top = drm.DrawBuffer(rgds::ScreenId::Top);
  auto &bottom = drm.DrawBuffer(rgds::ScreenId::Bottom);
  rgds::DrmRuntime::Clear(top, rgds::Rgba{12, 24, 34, 255});
  rgds::DrmRuntime::Clear(bottom, rgds::Rgba{20, 32, 44, 255});
  DrawCentered(top, 58, prompt.verb, 14, rgds::Rgba{155, 220, 255, 255});
  DrawCentered(top, 170, prompt.line1, 12, rgds::Rgba{255, 255, 255, 255});
  DrawCentered(top, 280, prompt.line2, 18, rgds::Rgba{255, 240, 185, 255});
  DrawCentered(bottom, 62, "JOYSTICK MAP", 10, rgds::Rgba{155, 220, 255, 255});
  DrawCentered(bottom, 178, std::to_string(index + 1) + " " + std::to_string(total), 14,
               rgds::Rgba{255, 255, 255, 255});
  DrawCentered(bottom, 292, std::to_string(countdown), 18, rgds::Rgba{255, 240, 185, 255});
  rgds::DrmRuntime::StrokeRect(top, 8, 8, rgds::kScreenW - 16, rgds::kScreenH - 16, 5,
                               rgds::Rgba{110, 215, 255, 255});
  rgds::DrmRuntime::StrokeRect(bottom, 8, 8, rgds::kScreenW - 16, rgds::kScreenH - 16, 5,
                               rgds::Rgba{110, 215, 255, 255});
  drm.Present();
}

void DrawDone(rgds::DrmRuntime &drm) {
  auto &top = drm.DrawBuffer(rgds::ScreenId::Top);
  auto &bottom = drm.DrawBuffer(rgds::ScreenId::Bottom);
  rgds::DrmRuntime::Clear(top, rgds::Rgba{12, 34, 26, 255});
  rgds::DrmRuntime::Clear(bottom, rgds::Rgba{18, 42, 32, 255});
  DrawCentered(top, 150, "DONE", 20, rgds::Rgba{220, 255, 225, 255});
  DrawCentered(top, 285, "LOG SAVED", 12, rgds::Rgba{180, 230, 190, 255});
  DrawCentered(bottom, 180, "EXITING", 14, rgds::Rgba{220, 255, 225, 255});
  drm.Present();
}

AxisState *FindAxis(Device &device, int code) {
  for (auto &axis : device.axes) {
    if (axis.code == code) return &axis;
  }
  return nullptr;
}

std::string CodeName(int code) {
  switch (code) {
    case BTN_THUMBL: return "BTN_THUMBL";
    case BTN_THUMBR: return "BTN_THUMBR";
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
    default: return "CODE_" + std::to_string(code);
  }
}

std::string AbsName(int code) {
  switch (code) {
    case ABS_X: return "ABS_X";
    case ABS_Y: return "ABS_Y";
    case ABS_Z: return "ABS_Z";
    case ABS_RX: return "ABS_RX";
    case ABS_RY: return "ABS_RY";
    case ABS_RZ: return "ABS_RZ";
    case ABS_HAT0X: return "ABS_HAT0X";
    case ABS_HAT0Y: return "ABS_HAT0Y";
    case ABS_HAT1X: return "ABS_HAT1X";
    case ABS_HAT1Y: return "ABS_HAT1Y";
    case ABS_HAT2X: return "ABS_HAT2X";
    case ABS_HAT2Y: return "ABS_HAT2Y";
    case ABS_HAT3X: return "ABS_HAT3X";
    case ABS_HAT3Y: return "ABS_HAT3Y";
    default: return "ABS_" + std::to_string(code);
  }
}

std::string AxisDirection(const AxisState &axis, int value) {
  const int center = axis.current;
  if (value < center) return "negative";
  if (value > center) return "positive";
  return "center";
}

int AxisThreshold(const AxisState &axis) {
  const int range = axis.maximum - axis.minimum;
  if (range <= 4) return 0;
  return std::max({8, range / 5, axis.flat * 2});
}

bool AxisMoved(const AxisState &axis, int value) {
  return std::abs(value - axis.current) > AxisThreshold(axis);
}

std::string AbsLine(const Prompt &prompt, const Device &device, const AxisState &axis, int value) {
  std::ostringstream out;
  out << "[joystick_map] prompt=" << prompt.key
      << " device=" << device.path
      << " name=\"" << device.name << "\""
      << " type=EV_ABS"
      << " code=" << axis.code
      << " abs_name=" << AbsName(axis.code)
      << " value=" << value
      << " center=" << axis.current
      << " min=" << axis.minimum
      << " max=" << axis.maximum
      << " flat=" << axis.flat
      << " direction=" << AxisDirection(axis, value);
  return out.str();
}

std::string KeyLine(const Prompt &prompt, const Device &device, const input_event &ev) {
  std::ostringstream out;
  out << "[joystick_map] prompt=" << prompt.key
      << " device=" << device.path
      << " name=\"" << device.name << "\""
      << " type=EV_KEY"
      << " code=" << ev.code
      << " code_name=" << CodeName(ev.code)
      << " value=" << ev.value;
  return out.str();
}

std::vector<Device> OpenDevices() {
  std::vector<Device> devices;
  for (int i = 0; i < 32; ++i) {
    Device device;
    device.path = "/dev/input/event" + std::to_string(i);
    device.fd = open(device.path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (device.fd < 0) continue;
    char name[128] = {};
    if (ioctl(device.fd, EVIOCGNAME(sizeof(name)), name) >= 0) device.name = name;
    for (int code : kAxisCodes) {
      input_absinfo info{};
      if (ioctl(device.fd, EVIOCGABS(code), &info) < 0) continue;
      AxisState axis;
      axis.code = code;
      axis.minimum = info.minimum;
      axis.maximum = info.maximum;
      axis.flat = info.flat;
      axis.current = info.value;
      axis.valid = true;
      device.axes.push_back(axis);
    }
    devices.push_back(device);
  }
  return devices;
}

void CloseDevices(std::vector<Device> &devices) {
  for (auto &device : devices) {
    if (device.fd >= 0) close(device.fd);
    device.fd = -1;
  }
}

void DrainDevices(std::vector<Device> &devices, int milliseconds) {
  const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(milliseconds);
  while (std::chrono::steady_clock::now() < end) {
    for (auto &device : devices) {
      input_event ev{};
      while (read(device.fd, &ev, sizeof(ev)) == static_cast<ssize_t>(sizeof(ev))) {
        if (ev.type == EV_ABS) {
          if (AxisState *axis = FindAxis(device, ev.code)) axis->current = ev.value;
        }
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}

bool ReadPromptEvents(std::vector<Device> &devices,
                      const Prompt &prompt,
                      std::string &result,
                      std::chrono::steady_clock::time_point &linger_until) {
  bool triggered = false;
  for (auto &device : devices) {
    input_event ev{};
    while (read(device.fd, &ev, sizeof(ev)) == static_cast<ssize_t>(sizeof(ev))) {
      if (ev.type == EV_KEY) {
        const std::string line = KeyLine(prompt, device, ev);
        std::cout << line << "\n";
        if (ev.value == 1 && (prompt.click || result.empty())) {
          result = line;
          triggered = true;
        }
      } else if (ev.type == EV_ABS) {
        AxisState *axis = FindAxis(device, ev.code);
        if (!axis) continue;
        const bool moved = AxisMoved(*axis, ev.value);
        const std::string line = AbsLine(prompt, device, *axis, ev.value);
        std::cout << line << "\n";
        if (!prompt.click && moved && result.empty()) {
          result = line;
          triggered = true;
        }
        if (!moved) {
          axis->current = ev.value;
        }
      }
    }
  }
  if (triggered) {
    linger_until = std::chrono::steady_clock::now() + std::chrono::milliseconds(450);
  }
  return triggered;
}

}  // namespace

int main() {
  rgds::DrmRuntime drm;
  const rgds::DrmInitResult init = drm.Initialize();
  if (!init.ok) {
    std::cerr << "[joystick_map] DRM init failed: " << init.error << "\n";
    return 2;
  }
  std::cout << "[joystick_map] DRM ready: " << drm.DescribeBindings() << "\n";

  std::vector<Device> devices = OpenDevices();
  for (const auto &device : devices) {
    std::cout << "[joystick_map] device=" << device.path << " name=\"" << device.name << "\"\n";
    for (const auto &axis : device.axes) {
      std::cout << "[joystick_map] axis device=" << device.path
                << " name=\"" << device.name << "\""
                << " code=" << axis.code
                << " abs_name=" << AbsName(axis.code)
                << " value=" << axis.current
                << " min=" << axis.minimum
                << " max=" << axis.maximum
                << " flat=" << axis.flat << "\n";
    }
  }

  std::vector<std::string> summary;
  for (size_t i = 0; i < kPrompts.size(); ++i) {
    DrainDevices(devices, 400);
    std::string result;
    auto linger_until = std::chrono::steady_clock::time_point{};
    bool done = false;
    for (int countdown = kPrompts[i].timeout_seconds; countdown > 0 && !done; --countdown) {
      DrawPrompt(drm, kPrompts[i], static_cast<int>(i), static_cast<int>(kPrompts.size()), countdown);
      const auto second_end = std::chrono::steady_clock::now() + std::chrono::seconds(1);
      while (std::chrono::steady_clock::now() < second_end) {
        const bool triggered = ReadPromptEvents(devices, kPrompts[i], result, linger_until);
        if (triggered) {
          while (std::chrono::steady_clock::now() < linger_until) {
            ReadPromptEvents(devices, kPrompts[i], result, linger_until);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
          }
          done = true;
          break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
    }
    if (result.empty()) {
      result = "[joystick_map] prompt=" + kPrompts[i].key + " result=TIMEOUT";
    }
    summary.push_back(result);
    std::cout << "[joystick_map] prompt=" << kPrompts[i].key << " done\n";
  }

  std::cout << "[joystick_map] summary_begin\n";
  for (const std::string &line : summary) {
    std::cout << line << "\n";
  }
  std::cout << "[joystick_map] summary_end\n";

  DrawDone(drm);
  std::this_thread::sleep_for(std::chrono::seconds(2));
  CloseDevices(devices);
  drm.Shutdown(false);
  return 0;
}
