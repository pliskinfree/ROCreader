#pragma once

#include "input_manager.h"
#include "scene_manager.h"

#include <cstdint>

namespace rgds {

constexpr float kFocusFlashSec = 0.45f;

struct InteractionState {
  bool focus_top = true;
  uint32_t focus_flash_until = 0;
  bool menu_open = false;
  bool focus_before_menu_top = true;
};

struct InteractionResult {
  bool menu_key_consumed = false;
  bool select_key_consumed = false;
  bool quit_requested = false;
  bool play_change_sfx = false;
  bool play_back_sfx = false;
};

void EnterShelf(InteractionState &state);
void EnterReader(InteractionState &state);
void CloseReaderToShelf(InteractionState &state);
InteractionResult HandleFrameInput(InteractionState &state, const InputManager &input, AppScene scene,
                                   uint32_t now);
bool RoutesInputToTop(const InteractionState &state);
bool RoutesInputToMenu(const InteractionState &state, AppScene scene);
bool ReaderMenuVisible(const InteractionState &state);

} // namespace rgds
