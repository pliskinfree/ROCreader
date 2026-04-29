#include "exit_panel.h"

#include "app_language.h"

#include <algorithm>

bool HandleExitPanelConfirm(SettingsRuntimeInputDeps &deps) {
  if (deps.on_exit_app) deps.on_exit_app();
  return true;
}

void DrawExitPanel(SettingsRuntimeRenderDeps &deps, SDL_Rect preview_rect, int language_index) {
  if (!deps.get_title_text_texture) return;
  const SDL_Color hint_color{240, 246, 255, 255};
  const std::string exit_hint = LocalizedAppText(language_index, AppTextId::ExitHint);
  TextCacheEntry *hint_tex = deps.get_title_text_texture(exit_hint, hint_color);
  if (!hint_tex || !hint_tex->texture) {
    hint_tex = deps.get_text_texture ? deps.get_text_texture(exit_hint, hint_color) : nullptr;
  }
  if (!hint_tex || !hint_tex->texture) return;

  SDL_Rect dst{
      preview_rect.x + std::max(0, (preview_rect.w - hint_tex->w) / 2),
      preview_rect.y + std::max(0, (preview_rect.h - hint_tex->h) / 2),
      hint_tex->w,
      hint_tex->h,
  };
  SDL_RenderCopy(deps.renderer, hint_tex->texture, nullptr, &dst);
}
