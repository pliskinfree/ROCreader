#pragma once

#include "settings_runtime.h"

bool HandleUpdatePanelInput(SettingsRuntimeInputDeps &deps);
void DrawUpdatePanel(SettingsRuntimeRenderDeps &deps, SDL_Rect preview_rect,
                     int language_index, float scale);
