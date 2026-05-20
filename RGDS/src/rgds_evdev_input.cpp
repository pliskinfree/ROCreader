#include "rgds_evdev_input.h"

#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <sstream>

namespace rgds {

EvdevInput::~EvdevInput() {
  CloseAll();
}

void EvdevInput::OpenAll() {
  CloseAll();
  for (int i = 0; i < 32; ++i) {
    Device device;
    device.path = "/dev/input/event" + std::to_string(i);
    device.fd = open(device.path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (device.fd < 0) continue;
    char name[128] = {};
    if (ioctl(device.fd, EVIOCGNAME(sizeof(name)), name) >= 0) {
      device.name = name;
    }
    devices_.push_back(device);
  }
}

void EvdevInput::CloseAll() {
  for (auto &device : devices_) {
    if (device.fd >= 0) {
      close(device.fd);
      device.fd = -1;
    }
  }
  devices_.clear();
}

bool EvdevInput::Poll(InputAction &action) {
  bool hit = false;
  for (auto &device : devices_) {
    hit = ReadDevice(device, action) || hit;
  }
  return hit;
}

std::string EvdevInput::DescribeDevices() const {
  std::ostringstream out;
  for (const auto &device : devices_) {
    out << device.path << "(" << device.name << ") ";
  }
  return out.str();
}

bool EvdevInput::ReadDevice(Device &device, InputAction &action) {
  bool hit = false;
  input_event ev{};
  while (true) {
    const ssize_t n = read(device.fd, &ev, sizeof(ev));
    if (n == static_cast<ssize_t>(sizeof(ev))) {
      if (ev.type == EV_KEY && ev.value == 1) {
        ApplyKeyCode(ev.code, action);
        hit = true;
      } else if (ev.type == EV_ABS) {
        ApplyAbsCode(device, ev.code, ev.value, action);
        hit = true;
      }
      continue;
    }
    if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;
    if (n <= 0) break;
    break;
  }
  return hit;
}

void EvdevInput::ApplyKeyCode(int code, InputAction &action) {
  // RGDS physical map captured from ANBERNIC-rk3568-keys on 2026-05-17.
  // Keep physical labels as captured; do not remap L2 into logical Select.
  if (code == 304) {
    action.confirm = true;  // A
  } else if (code == 305) {
    action.back = true;  // B
  } else if (code == 307) {
    action.menu = true;  // X, temporary menu/open auxiliary command
  } else if (code == 306) {
    action.menu = true;  // Y
  } else if (code == 308) {
    action.l1 = true;  // L1
  } else if (code == 309) {
    action.r1 = true;  // R1
  } else if (code == 314) {
    action.l2 = true;  // L2
  } else if (code == 315) {
    action.r2 = true;  // R2
  } else if (code == 310) {
    action.select = true;  // physical Select
  } else if (code == 311) {
    action.menu = true;  // physical Start
  } else if (code == 312) {
    action.exit_app = true;  // RG/front-end exit key
  } else if (code == 354) {
    action.volume_up = true;
  } else if (code == KEY_VOLUMEDOWN || code == 114) {
    action.volume_down = true;
  } else if (code == KEY_BACK) {
    action.menu = true;  // RGDS physical Menu on adc-keys.
  } else if (code == KEY_UP || code == BTN_DPAD_UP) {
    action.up = true;
  } else if (code == KEY_DOWN || code == BTN_DPAD_DOWN) {
    action.down = true;
  } else if (code == KEY_LEFT || code == BTN_DPAD_LEFT) {
    action.left = true;
  } else if (code == KEY_RIGHT || code == BTN_DPAD_RIGHT) {
    action.right = true;
  } else if (code == KEY_ENTER || code == KEY_SPACE || code == BTN_A || code == BTN_EAST) {
    action.confirm = true;
  } else if (code == KEY_ESC || code == BTN_B || code == BTN_SOUTH) {
    action.back = true;
  } else if (code == KEY_SELECT || code == BTN_SELECT) {
    action.select = true;
  } else if (code == KEY_MENU || code == KEY_HOMEPAGE || code == BTN_START) {
    action.menu = true;
  }
}

void EvdevInput::ApplyAbsCode(Device &device, int code, int value, InputAction &action) {
  // RGDS analog axes captured from ANBERNIC-rk3568-keys on 2026-05-20.
  if (code == ABS_HAT0X) {
    if (device.abs_hat_x_seen && value == device.abs_hat_x) {
      return;
    }
    if (value == 0) {
      device.abs_hat_x = value;
      device.abs_hat_x_seen = true;
      return;
    }
    if (value < 0) action.left = true;
    if (value > 0) action.right = true;
    device.abs_hat_x = value;
    device.abs_hat_x_seen = true;
  } else if (code == ABS_HAT0Y) {
    if (device.abs_hat_y_seen && value == device.abs_hat_y) {
      return;
    }
    if (value == 0) {
      device.abs_hat_y = value;
      device.abs_hat_y_seen = true;
      return;
    }
    if (value < 0) action.up = true;
    if (value > 0) action.down = true;
    device.abs_hat_y = value;
    device.abs_hat_y_seen = true;
  } else if (code == ABS_Z) {
    if (!device.abs_z_seen) {
      device.abs_z = value;
      device.abs_z_seen = true;
      return;
    }
    if (value < device.abs_z) action.left = true;
    if (value > device.abs_z) action.right = true;
    device.abs_z = value;
  } else if (code == ABS_RX) {
    if (!device.abs_rx_seen) {
      device.abs_rx = value;
      device.abs_rx_seen = true;
      return;
    }
    if (value < device.abs_rx) action.up = true;
    if (value > device.abs_rx) action.down = true;
    device.abs_rx = value;
  } else if (code == ABS_RY) {
    if (!device.abs_ry_seen) {
      device.abs_ry = value;
      device.abs_ry_seen = true;
      return;
    }
    if (value < device.abs_ry) action.left = true;
    if (value > device.abs_ry) action.right = true;
    device.abs_ry = value;
  } else if (code == ABS_RZ) {
    if (!device.abs_rz_seen) {
      device.abs_rz = value;
      device.abs_rz_seen = true;
      return;
    }
    if (value < device.abs_rz) action.up = true;
    if (value > device.abs_rz) action.down = true;
    device.abs_rz = value;
  }
}

}  // namespace rgds
