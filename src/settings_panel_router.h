#pragma once

#include "settings_runtime.h"

bool HandleSelectedSettingsPanelInput(SettingId id, SettingsRuntimeInputDeps &deps);
bool HandleSelectedSettingsPanelConfirm(SettingId id, SettingsRuntimeInputDeps &deps);
void DrawSelectedSettingsPanel(SettingId selected, SettingsRuntimeRenderDeps &deps,
                               SDL_Rect preview_rect, int language_index,
                               int first_menu_item_y, int sidebar_item_pitch,
                               int sidebar_item_h, float scale);
