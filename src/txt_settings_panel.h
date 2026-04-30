#pragma once

#include "menu_panel.h"
#include "settings_runtime.h"

class TxtSettingsPanel final : public IMenuPanel {
public:
  bool HandleInput(SettingsRuntimeInputDeps &deps) override;
  void Draw(const MenuPanelDrawContext &context) override;
};

bool HandleTxtSettingsPanelInput(SettingsRuntimeInputDeps &deps);
void DrawTxtSettingsPanel(SettingsRuntimeRenderDeps &deps, SDL_Rect preview_rect,
                          int language_index, int first_menu_item_y,
                          int sidebar_item_pitch, int sidebar_item_h, float scale);
