#pragma once

#include <linux/input.h>

#include <string>
#include <vector>

namespace rgds {

struct InputAction {
  bool select = false;
  bool menu = false;
  bool back = false;
  bool confirm = false;
  bool up = false;
  bool down = false;
  bool left = false;
  bool right = false;
  bool l1 = false;
  bool r1 = false;
  bool l2 = false;
  bool r2 = false;
  bool exit_app = false;
  bool volume_up = false;
  bool volume_down = false;
};

class EvdevInput {
 public:
  EvdevInput() = default;
  ~EvdevInput();

  EvdevInput(const EvdevInput &) = delete;
  EvdevInput &operator=(const EvdevInput &) = delete;

  void OpenAll();
  void CloseAll();
  bool Poll(InputAction &action);
  std::string DescribeDevices() const;

 private:
  struct Device {
    int fd = -1;
    std::string path;
    std::string name;
    int abs_x = 0;
    int abs_y = 0;
    int abs_hat_x = 0;
    int abs_hat_y = 0;
    bool abs_hat_x_seen = false;
    bool abs_hat_y_seen = false;
    bool abs_x_seen = false;
    bool abs_y_seen = false;
  };

  bool ReadDevice(Device &device, InputAction &action);
  static void ApplyKeyCode(int code, InputAction &action);
  static void ApplyAbsCode(Device &device, int code, int value, InputAction &action);

  std::vector<Device> devices_;
};

}  // namespace rgds
