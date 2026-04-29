#include "avatar_panel.h"

#include "contributor_avatar_runtime.h"

bool HandleAvatarPanelInput(SettingsRuntimeInputDeps &deps) {
  return HandleContributorAvatarInput(deps.input, deps.dt, deps.contributor_avatar_state,
                                      deps.contributor_avatar_count,
                                      deps.on_contributor_avatar_confirm);
}

void DrawAvatarPanel(SettingsRuntimeRenderDeps &deps, SDL_Rect preview_rect,
                     int language_index, float scale) {
  if (deps.contributor_avatar_entries.empty()) return;
  DrawContributorAvatarPreview(ContributorAvatarRenderDeps{
      deps.renderer,
      preview_rect,
      deps.contributor_avatar_entries,
      deps.contributor_avatar_state,
      language_index,
      scale,
      deps.draw_rect,
      deps.get_text_texture,
  });
}
