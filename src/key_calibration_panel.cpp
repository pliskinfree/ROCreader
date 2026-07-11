#include "key_calibration_panel.h"

#include "key_calibration_runtime.h"

bool HandleKeyCalibrationPanelInput(SettingsRuntimeInputDeps &deps) {
  return HandleKeyCalibrationInput(deps.input, deps.input_profile, deps.key_calibration_state,
                                   deps.key_calibration_callbacks);
}

void DrawKeyCalibrationPanel(SettingsRuntimeRenderDeps &deps, SDL_Rect preview_rect,
                             int language_index, float scale) {
  DrawKeyCalibrationPreview(KeyCalibrationRenderDeps{
      deps.renderer,
      preview_rect,
      deps.key_calibration_state,
      deps.cfg.theme != 0,
      language_index,
      scale,
      deps.services.draw_rect,
      deps.services.get_text_texture,
      deps.services.get_title_text_texture,
  });
}
