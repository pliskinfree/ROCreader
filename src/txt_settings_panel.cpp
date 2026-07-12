#include "txt_settings_panel.h"

#include "txt_settings_runtime.h"

bool TxtSettingsPanel::HandleInput(SettingsRuntimeInputDeps &deps) {
  return HandleTxtSettingsInput(deps.input, deps.txt_settings_state, deps.txt_settings_callbacks);
}

void TxtSettingsPanel::Draw(const MenuPanelDrawContext &context) {
  SettingsRuntimeRenderDeps &deps = context.deps;
  DrawTxtSettingsPreview(TxtSettingsRenderDeps{
      deps.renderer,
      context.preview_rect,
      deps.txt_settings_state,
      deps.txt_transcode_job,
      deps.cfg.theme != 0,
      context.language_index,
      context.first_menu_item_y,
      context.sidebar_item_pitch,
      context.sidebar_item_h,
      context.scale,
      deps.input_profile == InputProfile::GKD350HUltra,
      deps.services.draw_rect,
      deps.services.get_text_texture,
      deps.services.get_title_text_texture,
      deps.services.utf8_ellipsize,
  });
}

bool HandleTxtSettingsPanelInput(SettingsRuntimeInputDeps &deps) {
  TxtSettingsPanel panel;
  return panel.HandleInput(deps);
}

void DrawTxtSettingsPanel(SettingsRuntimeRenderDeps &deps, SDL_Rect preview_rect,
                          int language_index, int first_menu_item_y,
                          int sidebar_item_pitch, int sidebar_item_h, float scale) {
  TxtSettingsPanel panel;
  panel.Draw(MenuPanelDrawContext{deps, preview_rect, language_index, first_menu_item_y,
                                  sidebar_item_pitch, sidebar_item_h, scale});
}
