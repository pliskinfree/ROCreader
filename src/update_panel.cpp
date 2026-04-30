#include "update_panel.h"

#include "version_update_runtime.h"

bool HandleUpdatePanelInput(SettingsRuntimeInputDeps &deps) {
  return HandleVersionUpdateInput(deps.input, deps.version_update_state, deps.version_update_callbacks);
}

void DrawUpdatePanel(SettingsRuntimeRenderDeps &deps, SDL_Rect preview_rect,
                     int language_index, float scale) {
  DrawVersionUpdatePreview(VersionUpdateRenderDeps{
      deps.renderer,
      preview_rect,
      deps.version_update_state,
      deps.cfg.theme != 0,
      language_index,
      scale,
      deps.services.draw_rect,
      deps.services.get_text_texture,
      deps.services.get_title_text_texture,
  });
}
