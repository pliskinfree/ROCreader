#include "volume_overlay.h"

#include <string>

void DrawVolumeOverlay(const VolumeOverlayRenderDeps &deps) {
#ifdef HAVE_SDL2_TTF
  if (!deps.renderer || deps.now > deps.display_until) return;
  SDL_Color volume_text{238, 242, 250, 255};
  const std::string label = std::to_string(deps.display_percent);
  TextCacheEntry *te = deps.get_text_texture ? deps.get_text_texture(label, volume_text) : nullptr;
  if (!te || !te->texture) return;
  const int tx = deps.scale_px ? deps.scale_px(18) : 18;
  const int ty = deps.top_bar_y + std::max(0, (deps.top_bar_h - te->h) / 2);
  SDL_Rect td{tx, ty, te->w, te->h};
  SDL_RenderCopy(deps.renderer, te->texture, nullptr, &td);
#else
  (void)deps;
#endif
}

