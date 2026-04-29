#include "system_controls_panel.h"

#include "system_settings_runtime.h"

bool HandleSystemControlsPanelInput(SettingsRuntimeInputDeps &deps) {
  return HandleSystemSettingsInput(deps.input, deps.system_settings_state, deps.system_settings_callbacks);
}

void DrawSystemControlsPanel(SettingsRuntimeRenderDeps &deps, SDL_Rect preview_rect,
                             int first_menu_item_y, int sidebar_item_pitch,
                             int sidebar_item_h, float scale) {
  DrawSystemSettingsPreview(SystemSettingsRenderDeps{
      deps.renderer,
      preview_rect,
      deps.system_settings_state,
      deps.cfg.theme != 0,
      first_menu_item_y,
      sidebar_item_pitch,
      sidebar_item_h,
      scale,
      deps.draw_rect,
      deps.get_text_texture,
      deps.get_title_text_texture,
  });
}
