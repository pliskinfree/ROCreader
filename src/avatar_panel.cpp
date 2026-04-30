#include "avatar_panel.h"

#include "contributor_avatar_runtime.h"

bool AvatarPanel::HandleInput(SettingsRuntimeInputDeps &deps) {
  return HandleContributorAvatarInput(deps.input, deps.dt, deps.contributor_avatar_state,
                                      deps.contributor_avatar_count,
                                      deps.actions.on_contributor_avatar_confirm);
}

void AvatarPanel::Draw(const MenuPanelDrawContext &context) {
  SettingsRuntimeRenderDeps &deps = context.deps;
  if (deps.contributor_avatar_entries.empty()) return;
  DrawContributorAvatarPreview(ContributorAvatarRenderDeps{
      deps.renderer,
      context.preview_rect,
      deps.contributor_avatar_entries,
      deps.contributor_avatar_state,
      context.language_index,
      context.scale,
      deps.services.draw_rect,
      deps.services.get_text_texture,
  });
}

bool HandleAvatarPanelInput(SettingsRuntimeInputDeps &deps) {
  AvatarPanel panel;
  return panel.HandleInput(deps);
}

void DrawAvatarPanel(SettingsRuntimeRenderDeps &deps, SDL_Rect preview_rect,
                     int language_index, float scale) {
  AvatarPanel panel;
  panel.Draw(MenuPanelDrawContext{deps, preview_rect, language_index, 0, 0, 0, scale});
}
