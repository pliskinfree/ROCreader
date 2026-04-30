#include "system_controls_panel.h"

#include "system_settings_runtime.h"

bool SystemControlsPanel::HandleInput(SettingsRuntimeInputDeps &deps) {
  return HandleSystemSettingsInput(deps.input, deps.system_settings_state, deps.system_settings_callbacks);
}

void SystemControlsPanel::Draw(const MenuPanelDrawContext &context) {
  SettingsRuntimeRenderDeps &deps = context.deps;
  DrawSystemSettingsPreview(SystemSettingsRenderDeps{
      deps.renderer,
      context.preview_rect,
      deps.system_settings_state,
      deps.cfg.theme != 0,
      context.first_menu_item_y,
      context.sidebar_item_pitch,
      context.sidebar_item_h,
      context.scale,
      deps.services.draw_rect,
      deps.services.get_text_texture,
      deps.services.get_title_text_texture,
  });
}

bool HandleSystemControlsPanelInput(SettingsRuntimeInputDeps &deps) {
  SystemControlsPanel panel;
  return panel.HandleInput(deps);
}

void DrawSystemControlsPanel(SettingsRuntimeRenderDeps &deps, SDL_Rect preview_rect,
                             int first_menu_item_y, int sidebar_item_pitch,
                             int sidebar_item_h, float scale) {
  SystemControlsPanel panel;
  panel.Draw(MenuPanelDrawContext{deps, preview_rect, 0, first_menu_item_y, sidebar_item_pitch,
                                  sidebar_item_h, scale});
}
