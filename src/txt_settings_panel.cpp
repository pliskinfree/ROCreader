#include "txt_settings_panel.h"

#include "txt_settings_runtime.h"

bool HandleTxtSettingsPanelInput(SettingsRuntimeInputDeps &deps) {
  return HandleTxtSettingsInput(deps.input, deps.txt_settings_state, deps.txt_settings_callbacks);
}

void DrawTxtSettingsPanel(SettingsRuntimeRenderDeps &deps, SDL_Rect preview_rect,
                          int language_index, int first_menu_item_y,
                          int sidebar_item_pitch, int sidebar_item_h, float scale) {
  DrawTxtSettingsPreview(TxtSettingsRenderDeps{
      deps.renderer,
      preview_rect,
      deps.txt_settings_state,
      deps.txt_transcode_job,
      deps.cfg.theme != 0,
      language_index,
      first_menu_item_y,
      sidebar_item_pitch,
      sidebar_item_h,
      scale,
      deps.draw_rect,
      deps.get_text_texture,
      deps.get_title_text_texture,
      deps.utf8_ellipsize,
  });
}
