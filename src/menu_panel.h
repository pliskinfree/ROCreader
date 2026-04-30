#pragma once

#include "settings_runtime.h"

struct MenuPanelDrawContext {
  SettingsRuntimeRenderDeps &deps;
  SDL_Rect preview_rect{};
  int language_index = 0;
  int first_menu_item_y = 0;
  int sidebar_item_pitch = 0;
  int sidebar_item_h = 0;
  float scale = 1.0f;
};

class IMenuPanel {
public:
  virtual ~IMenuPanel() = default;

  virtual bool HandleInput(SettingsRuntimeInputDeps &deps) {
    (void)deps;
    return false;
  }

  virtual bool Confirm(SettingsRuntimeInputDeps &deps) {
    (void)deps;
    return false;
  }

  virtual void Draw(const MenuPanelDrawContext &context) {
    (void)context;
  }
};
