#pragma once

#include "settings_runtime.h"

bool HandleSystemControlsPanelInput(SettingsRuntimeInputDeps &deps);
void DrawSystemControlsPanel(SettingsRuntimeRenderDeps &deps, SDL_Rect preview_rect,
                             int first_menu_item_y, int sidebar_item_pitch,
                             int sidebar_item_h, float scale);
