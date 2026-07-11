#include "rgds_interaction.h"

#include <SDL.h>

namespace rgds {

void EnterShelf(InteractionState &state) {
  state.menu_open = true;
  state.focus_top = true;
}

void EnterReader(InteractionState &state) {
  state.menu_open = false;
  state.focus_top = true;
  state.focus_before_menu_top = true;
}

void CloseReaderToShelf(InteractionState &state) {
  EnterShelf(state);
}

InteractionResult HandleFrameInput(InteractionState &state, const InputManager &input, AppScene scene,
                                   uint32_t now) {
  InteractionResult result;
  if (input.IsJustPressed(Button::Select)) {
    state.focus_top = !state.focus_top;
    state.focus_flash_until = now + static_cast<uint32_t>(kFocusFlashSec * 1000.0f);
    result.select_key_consumed = true;
    result.play_change_sfx = true;
  }

  if (scene == AppScene::Shelf) {
    state.menu_open = true;
  }

  if (input.IsJustPressed(Button::Menu)) {
    result.menu_key_consumed = true;
    if (scene == AppScene::Reader) {
      if (state.menu_open) {
        state.menu_open = false;
        state.focus_top = state.focus_before_menu_top;
        result.play_back_sfx = true;
      } else {
        state.focus_before_menu_top = state.focus_top;
        state.menu_open = true;
        state.focus_top = false;
        result.play_back_sfx = true;
      }
    } else if (scene == AppScene::Shelf) {
      state.focus_top = false;
      state.focus_flash_until = now + static_cast<uint32_t>(kFocusFlashSec * 1000.0f);
      result.play_change_sfx = true;
    }
  }

  return result;
}

bool RoutesInputToTop(const InteractionState &state) {
  return state.focus_top;
}

bool RoutesInputToMenu(const InteractionState &state, AppScene scene) {
  if (scene == AppScene::Shelf) return !state.focus_top;
  return scene == AppScene::Reader && state.menu_open && !state.focus_top;
}

bool ReaderMenuVisible(const InteractionState &state) {
  return state.menu_open && !state.focus_top;
}

} // namespace rgds
