#pragma once

#include <SDL.h>

#include <array>
#include <string>

constexpr int kButtonCount = 18;

enum class Button {
  Up,
  Down,
  Left,
  Right,
  A,
  B,
  X,
  Y,
  Menu,
  L1,
  L2,
  R1,
  R2,
  Start,
  Select,
  VolUp,
  VolDown,
  Power,
};

enum class InputProfile {
  DesktopDefault,
  H700Default,
  H70034xxSp,
  TrimuiBrick,
};

const char *ButtonName(Button b);
const char *InputProfileName(InputProfile profile);
const char *SdlEventName(Uint32 type);

struct BtnState {
  bool down = false;
  bool just_pressed = false;
  bool just_released = false;
  bool repeated = false;
  bool long_pressed = false;
  float hold_time = 0.0f;
  float repeat_timer = 0.0f;
  bool repeat_active = false;
};

class InputManager {
public:
  InputManager(const std::string &mapping_path, InputProfile input_profile);

  void BeginFrame(float dt);
  void HandleEvent(const SDL_Event &e);
  void EndFrame();

  bool IsPressed(Button b) const;
  bool IsJustPressed(Button b) const;
  bool IsJustReleased(Button b) const;
  bool IsRepeated(Button b) const;
  bool IsLongPressed(Button b) const;
  float HoldTime(Button b) const;
  bool AnyPressed() const;
  void ResetAll();
  std::string DescribeJoyMap() const;
  std::string DescribePadMap() const;

private:
  static Button InvalidButton();
public:
  static bool IsValid(Button b);
private:
  static Button KeyToButton(SDL_Keycode k);
  Button PadToButton(uint8_t b) const;
  Button JoyButtonToButton(uint8_t b) const;
  void LoadDefaultPadMap(InputProfile input_profile);
  void LoadDefaultJoyMap(InputProfile input_profile);
  static std::string Trim(std::string s);
  static bool ParseButtonName(const std::string &raw, Button &out);
  void LoadOverrides(const std::string &mapping_path);
  void SetDown(Button b, bool down);
  const BtnState &Get(Button b) const;
  std::string DescribeMap(const std::array<Button, 32> &map, const char *prefix) const;

  std::array<BtnState, kButtonCount> states_{};
  std::array<Button, 32> pad_map_{};
  std::array<Button, 32> joy_map_{};
  InputProfile input_profile_ = InputProfile::DesktopDefault;
  std::array<int, 16> last_pad_axis_values_{};
  std::array<int, 16> last_joy_axis_values_{};
  std::array<bool, 16> pad_axis_seen_{};
  std::array<bool, 16> joy_axis_seen_{};
  float dt_ = 1.0f / 60.0f;
};
