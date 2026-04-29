#pragma once

#include "settings_runtime.h"

bool HandleAvatarPanelInput(SettingsRuntimeInputDeps &deps);
void DrawAvatarPanel(SettingsRuntimeRenderDeps &deps, SDL_Rect preview_rect,
                     int language_index, float scale);
