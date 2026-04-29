#pragma once

#include "settings_runtime.h"

bool HandleExitPanelConfirm(SettingsRuntimeInputDeps &deps);
void DrawExitPanel(SettingsRuntimeRenderDeps &deps, SDL_Rect preview_rect, int language_index);
