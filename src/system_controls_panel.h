#pragma once

#include "menu_panel.h"
#include "settings_runtime.h"

class SystemControlsPanel final : public IMenuPanel {
public:
  bool HandleInput(SettingsRuntimeInputDeps &deps) override;
  void Draw(const MenuPanelDrawContext &context) override;
};

bool HandleSystemControlsPanelInput(SettingsRuntimeInputDeps &deps);
void DrawSystemControlsPanel(SettingsRuntimeRenderDeps &deps, SDL_Rect preview_rect,
                             int first_menu_item_y, int sidebar_item_pitch,
                             int sidebar_item_h, float scale);
