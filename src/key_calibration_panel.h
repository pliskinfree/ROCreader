#pragma once

#include "settings_runtime.h"

bool HandleKeyCalibrationPanelInput(SettingsRuntimeInputDeps &deps);
void DrawKeyCalibrationPanel(SettingsRuntimeRenderDeps &deps, SDL_Rect preview_rect,
                             int language_index, float scale);
