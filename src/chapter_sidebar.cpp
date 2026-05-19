#include "chapter_sidebar.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr Uint32 kChapterMarqueePauseMs = 750;
constexpr float kChapterMarqueeSpeedPx = 48.0f;

int Scale(const ChapterSidebarRenderDeps &deps, int value) {
  return deps.scale_px ? deps.scale_px(value) : value;
}

int VisibleRowCount(const ChapterSidebarRenderDeps &deps, int list_y, int row_pitch) {
  return std::max(1, (deps.screen_h - list_y - Scale(deps, 10)) / std::max(1, row_pitch));
}

void ClampSidebarWindow(ReaderUiState &ui, int visible_rows) {
  const int count = static_cast<int>(ui.chapter_anchors.size());
  if (count <= 0) {
    ui.chapter_sidebar_selected = 0;
    ui.chapter_sidebar_first_visible = 0;
    return;
  }
  ui.chapter_sidebar_selected = std::clamp(ui.chapter_sidebar_selected, 0, count - 1);
  if (ui.chapter_sidebar_selected < ui.chapter_sidebar_first_visible) {
    ui.chapter_sidebar_first_visible = ui.chapter_sidebar_selected;
  } else if (ui.chapter_sidebar_selected >= ui.chapter_sidebar_first_visible + visible_rows) {
    ui.chapter_sidebar_first_visible = ui.chapter_sidebar_selected - visible_rows + 1;
  }
  ui.chapter_sidebar_first_visible =
      std::clamp(ui.chapter_sidebar_first_visible, 0, std::max(0, count - visible_rows));
}

void ResetChapterMarquee(ReaderUiState &ui) {
  ui.chapter_marquee_selected = -1;
  ui.chapter_marquee_start_ticks = 0;
}

int EffectiveVisibleRows(const ReaderUiState &ui, int visible_rows) {
  if (!ui.chapter_loading) return visible_rows;
  return std::max(1, visible_rows - 1);
}

#ifdef HAVE_SDL2_TTF
void DrawChapterLabel(const ChapterSidebarRenderDeps &deps,
                      TextCacheEntry *label,
                      int index,
                      bool selected,
                      int text_x,
                      int y,
                      int row_h,
                      int max_w) {
  if (!label || !label->texture) return;

  const int text_y = y + std::max(0, (row_h - label->h) / 2);
  if (!selected || label->w <= max_w) {
    SDL_Rect src{0, 0, std::min(label->w, max_w), label->h};
    SDL_Rect dst{text_x, text_y, src.w, src.h};
    SDL_RenderCopy(deps.renderer, label->texture, &src, &dst);
    return;
  }

  const Uint32 now = SDL_GetTicks();
  if (deps.ui.chapter_marquee_selected != index) {
    deps.ui.chapter_marquee_selected = index;
    deps.ui.chapter_marquee_start_ticks = now;
  }

  const Uint32 elapsed_ms = now - deps.ui.chapter_marquee_start_ticks;
  float offset = 0.0f;
  if (elapsed_ms > kChapterMarqueePauseMs) {
    const int gap = Scale(deps, 24);
    const float span = static_cast<float>(label->w + gap);
    const float elapsed_sec = static_cast<float>(elapsed_ms - kChapterMarqueePauseMs) / 1000.0f;
    offset = span > 0.0f ? std::fmod(elapsed_sec * kChapterMarqueeSpeedPx, span) : 0.0f;
  }

  SDL_Rect previous_clip{};
  const SDL_bool had_clip = SDL_RenderIsClipEnabled(deps.renderer);
  SDL_RenderGetClipRect(deps.renderer, &previous_clip);

  SDL_Rect clip{text_x, y, max_w, row_h};
  SDL_RenderSetClipRect(deps.renderer, &clip);

  const int gap = Scale(deps, 24);
  const int xoff = static_cast<int>(offset + 0.5f);
  SDL_Rect dst1{text_x - xoff, text_y, label->w, label->h};
  SDL_Rect dst2{dst1.x + label->w + gap, text_y, label->w, label->h};
  SDL_RenderCopy(deps.renderer, label->texture, nullptr, &dst1);
  SDL_RenderCopy(deps.renderer, label->texture, nullptr, &dst2);

  SDL_RenderSetClipRect(deps.renderer, had_clip == SDL_TRUE ? &previous_clip : nullptr);
}
#endif
}  // namespace

bool HandleChapterSidebarInput(const ChapterSidebarInputDeps &deps) {
  if (!deps.ui.chapter_sidebar_visible) return false;
  const int count = static_cast<int>(deps.ui.chapter_anchors.size());
  if (deps.input.IsJustPressed(Button::Y) || deps.input.IsJustPressed(Button::B)) {
    deps.ui.chapter_sidebar_visible = false;
    deps.ui.chapter_loading = false;
    deps.ui.chapter_loading_pct = 100;
    ResetChapterMarquee(deps.ui);
    return true;
  }
  if (count > 0 && (deps.input.IsJustPressed(Button::Up) || deps.input.IsRepeated(Button::Up))) {
    deps.ui.chapter_sidebar_selected = (deps.ui.chapter_sidebar_selected - 1 + count) % count;
    ResetChapterMarquee(deps.ui);
    return true;
  }
  if (count > 0 && (deps.input.IsJustPressed(Button::Down) || deps.input.IsRepeated(Button::Down))) {
    deps.ui.chapter_sidebar_selected = (deps.ui.chapter_sidebar_selected + 1) % count;
    ResetChapterMarquee(deps.ui);
    return true;
  }
  if (count > 0 && (deps.input.IsJustPressed(Button::Left) || deps.input.IsRepeated(Button::Left))) {
    const int page_rows = std::max(1, deps.ui.chapter_sidebar_page_rows);
    deps.ui.chapter_sidebar_selected = std::max(0, deps.ui.chapter_sidebar_selected - page_rows);
    deps.ui.chapter_sidebar_first_visible =
        std::max(0, deps.ui.chapter_sidebar_first_visible - page_rows);
    ResetChapterMarquee(deps.ui);
    return true;
  }
  if (count > 0 && (deps.input.IsJustPressed(Button::Right) || deps.input.IsRepeated(Button::Right))) {
    const int page_rows = std::max(1, deps.ui.chapter_sidebar_page_rows);
    deps.ui.chapter_sidebar_selected = std::min(count - 1, deps.ui.chapter_sidebar_selected + page_rows);
    deps.ui.chapter_sidebar_first_visible += page_rows;
    ResetChapterMarquee(deps.ui);
    return true;
  }
  if (count > 0 && deps.input.IsJustPressed(Button::A)) {
    const int selected = std::clamp(deps.ui.chapter_sidebar_selected, 0, count - 1);
    if (deps.jump_to_chapter) deps.jump_to_chapter(deps.ui.chapter_anchors[selected]);
    deps.ui.chapter_sidebar_visible = false;
    deps.ui.chapter_loading = false;
    deps.ui.chapter_loading_pct = 100;
    ResetChapterMarquee(deps.ui);
    return true;
  }
  return true;
}

void DrawChapterSidebar(const ChapterSidebarRenderDeps &deps) {
  if (!deps.renderer || !deps.ui.chapter_sidebar_visible || !deps.draw_rect) return;
  const int w = std::max(96, deps.sidebar_w);
  const int title_y = Scale(deps, 10);
  const int divider_y = Scale(deps, 39);
  const int list_y = Scale(deps, 48);
  const int row_h = Scale(deps, 24);
  const int row_pitch = Scale(deps, 27);
  const int margin_x = Scale(deps, 8);
  const int text_x = Scale(deps, 18);
  const int indicator_w = Scale(deps, 3);
  const int visible_rows = EffectiveVisibleRows(deps.ui, VisibleRowCount(deps, list_y, row_pitch));
  deps.ui.chapter_sidebar_page_rows = visible_rows;
  ClampSidebarWindow(deps.ui, visible_rows);
  auto draw_rect = [&](int x, int y, int rw, int rh, SDL_Color color, bool filled) {
    deps.draw_rect(deps.origin_x + x, deps.origin_y + y, rw, rh, color, filled);
  };

  draw_rect(0, 0, w, deps.screen_h, SDL_Color{24, 34, 46, 255}, true);
  draw_rect(w - 1, 0, 1, deps.screen_h, SDL_Color{82, 125, 158, 255}, true);
  draw_rect(Scale(deps, 8), divider_y, w - Scale(deps, 16), 1, SDL_Color{66, 95, 124, 255}, true);

#ifdef HAVE_SDL2_TTF
  if (deps.get_text_texture) {
    const SDL_Color title_color{240, 246, 255, 255};
    TextCacheEntry *title = deps.get_text_texture(u8"\u7ae0\u8282", title_color);
    if (title && title->texture) {
      SDL_Rect dst{deps.origin_x + std::max(0, (w - title->w) / 2), deps.origin_y + title_y, title->w, title->h};
      SDL_RenderCopy(deps.renderer, title->texture, nullptr, &dst);
    }
  }
#endif

  const int count = static_cast<int>(deps.ui.chapter_anchors.size());
  const int end = std::min(count, deps.ui.chapter_sidebar_first_visible + visible_rows);
  int y = list_y;
  for (int i = deps.ui.chapter_sidebar_first_visible; i < end; ++i) {
    const bool selected = i == deps.ui.chapter_sidebar_selected;
    draw_rect(margin_x, y, w - margin_x * 2, row_h,
              selected ? SDL_Color{63, 119, 158, 255} : SDL_Color{42, 56, 74, 192}, true);
    if (selected) {
      draw_rect(margin_x, y, indicator_w, row_h, SDL_Color{139, 214, 255, 255}, true);
      draw_rect(margin_x - 1, y - 1, w - margin_x * 2 + 2, row_h + 2,
                SDL_Color{85, 152, 198, 208}, false);
    }
#ifdef HAVE_SDL2_TTF
    if (deps.get_text_texture) {
      const SDL_Color item_color{230, 236, 248, 255};
      TextCacheEntry *label = deps.get_text_texture(deps.ui.chapter_anchors[i].title, item_color);
      if (label && label->texture) {
        const int max_w = std::max(1, w - text_x - margin_x);
        DrawChapterLabel(deps, label, i, selected, deps.origin_x + text_x, deps.origin_y + y, row_h, max_w);
      }
    }
#endif
    y += row_pitch;
  }

  if (deps.ui.chapter_loading) {
    draw_rect(margin_x, y, w - margin_x * 2, row_h, SDL_Color{31, 42, 56, 208}, true);
#ifdef HAVE_SDL2_TTF
    if (deps.get_text_texture) {
      const SDL_Color loading_color{176, 195, 214, 255};
      const int pct = std::clamp(deps.ui.chapter_loading_pct, 0, 100);
      const std::string loading_text =
          u8"\u7ae0\u8282\u52a0\u8f7d\u4e2d " + std::to_string(pct) + "%";
      TextCacheEntry *label = deps.get_text_texture(loading_text, loading_color);
      if (label && label->texture) {
        const int max_w = std::max(1, w - text_x - margin_x);
        SDL_Rect src{0, 0, std::min(label->w, max_w), label->h};
        SDL_Rect dst{deps.origin_x + text_x, deps.origin_y + y + std::max(0, (row_h - label->h) / 2), src.w, src.h};
        SDL_RenderCopy(deps.renderer, label->texture, &src, &dst);
      }
    }
#endif
  }
}
