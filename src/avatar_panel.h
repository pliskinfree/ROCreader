#pragma once

#include "menu_panel.h"
#include "settings_runtime.h"

class AvatarPanel final : public IMenuPanel {
public:
  bool HandleInput(SettingsRuntimeInputDeps &deps) override;
  void Draw(const MenuPanelDrawContext &context) override;
};

bool HandleAvatarPanelInput(SettingsRuntimeInputDeps &deps);
void DrawAvatarPanel(SettingsRuntimeRenderDeps &deps, SDL_Rect preview_rect,
                     int language_index, float scale);
