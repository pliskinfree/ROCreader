#pragma once

#include "settings_runtime.h"

bool HandleOnlineSourcePanelInput(SettingsRuntimeInputDeps &deps);
void DrawOnlineSourcePanel(SettingsRuntimeRenderDeps &deps, SDL_Rect preview_rect,
                           int language_index, int first_menu_item_y,
                           int sidebar_item_pitch, int sidebar_item_h, float scale);
