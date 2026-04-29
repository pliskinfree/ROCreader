#include "reader_progress_controller.h"

#include "txt_reader_session.h"

#include <algorithm>
#include <cstdint>

namespace {
int ClampIntLocal(int value, int lo, int hi) {
  return std::max(lo, std::min(hi, value));
}
}  // namespace

int ReaderPageProgressPercent(int page_index, int page_count) {
  if (page_count <= 1) return 100;
  return ClampIntLocal(static_cast<int>((static_cast<int64_t>(ClampIntLocal(page_index, 0, page_count - 1)) * 100) /
                                        (page_count - 1)),
                       0, 100);
}

int ReaderPageIndexForPercent(int current_page, int target_pct, int page_count) {
  if (page_count <= 1) return 0;
  const int clamped_current = ClampIntLocal(current_page, 0, page_count - 1);
  const int clamped_target = ClampIntLocal(target_pct, 0, 100);
  const int current_pct = ReaderPageProgressPercent(clamped_current, page_count);
  if (clamped_target == current_pct) return clamped_current;

  if (clamped_target > current_pct) {
    for (int page = clamped_current + 1; page < page_count; ++page) {
      if (ReaderPageProgressPercent(page, page_count) >= clamped_target) {
        return page;
      }
    }
    return page_count - 1;
  }

  for (int page = clamped_current - 1; page >= 0; --page) {
    if (ReaderPageProgressPercent(page, page_count) <= clamped_target) {
      return page;
    }
  }
  return 0;
}

void ReaderJumpToPercent(ReaderProgressControllerDeps &deps, int pct) {
  ReaderMode reader_mode = deps.ui.mode;
  TxtReaderState &txt_reader = deps.ui.txt_reader;
  if (reader_mode == ReaderMode::Txt && txt_reader.open) {
    if (deps.text_jump_to_percent) deps.text_jump_to_percent(pct);
    return;
  }
  if (reader_mode == ReaderMode::Pdf && deps.pdf_runtime.IsOpen()) {
    deps.pdf_runtime.SetPage(ReaderPageIndexForPercent(deps.pdf_runtime.CurrentPage(),
                                                       pct,
                                                       std::max(1, deps.pdf_runtime.PageCount())));
    return;
  }
  if (reader_mode == ReaderMode::Epub && deps.epub_runtime.IsOpen()) {
    deps.epub_runtime.SetPage(ReaderPageIndexForPercent(deps.epub_runtime.CurrentPage(),
                                                        pct,
                                                        std::max(1, deps.epub_runtime.PageCount())));
    return;
  }
  if (reader_mode == ReaderMode::ZipImage && deps.zip_image_runtime.IsOpen()) {
    deps.zip_image_runtime.SetPage(ReaderPageIndexForPercent(deps.zip_image_runtime.CurrentPage(),
                                                             pct,
                                                             std::max(1, deps.zip_image_runtime.PageCount())));
  }
}

int CurrentReaderProgressPercent(const ReaderProgressControllerDeps &deps) {
  ReaderMode reader_mode = deps.ui.mode;
  const TxtReaderState &txt_reader = deps.ui.txt_reader;
  if (reader_mode == ReaderMode::Txt && txt_reader.open) {
    return TxtReaderProgressPercent(txt_reader);
  }
  if (reader_mode == ReaderMode::Pdf && deps.pdf_runtime.IsOpen()) {
    const int page_count = std::max(1, deps.pdf_runtime.PageCount());
    const int page_idx = ClampIntLocal(deps.pdf_runtime.Progress().page, 0, page_count - 1);
    return (page_count <= 1)
               ? 100
               : ClampIntLocal(static_cast<int>((static_cast<int64_t>(page_idx) * 100) / (page_count - 1)), 0, 100);
  }
  if (reader_mode == ReaderMode::Epub && deps.epub_runtime.IsOpen()) {
    const int page_count = std::max(1, deps.epub_runtime.PageCount());
    const int page_idx = ClampIntLocal(deps.epub_runtime.Progress().page, 0, page_count - 1);
    return (page_count <= 1)
               ? 100
               : ClampIntLocal(static_cast<int>((static_cast<int64_t>(page_idx) * 100) / (page_count - 1)), 0, 100);
  }
  if (reader_mode == ReaderMode::ZipImage && deps.zip_image_runtime.IsOpen()) {
    const int page_count = std::max(1, deps.zip_image_runtime.PageCount());
    const int page_idx = ClampIntLocal(deps.zip_image_runtime.Progress().page, 0, page_count - 1);
    return (page_count <= 1)
               ? 100
               : ClampIntLocal(static_cast<int>((static_cast<int64_t>(page_idx) * 100) / (page_count - 1)), 0, 100);
  }
  return 0;
}

int CurrentTxtLayoutProgressPercent(const ReaderUiState &ui) {
  if (!(ui.mode == ReaderMode::Txt && ui.txt_reader.open)) return 0;
  if (!ui.txt_reader.loading) return 100;
  const size_t total = ui.txt_reader.pending_raw.size();
  if (total == 0) return 0;
  return ClampIntLocal(static_cast<int>((static_cast<int64_t>(ui.txt_reader.parse_pos) * 100) / total), 0, 100);
}

void DrawReaderProgressOverlay(ReaderProgressOverlayRenderDeps &deps) {
#ifdef HAVE_SDL2_TTF
  if (!deps.renderer || !deps.progress.ui.progress_overlay_visible) return;
  auto scale_px = [&](int value) { return deps.scale_px ? deps.scale_px(value) : value; };

  ReaderUiState &ui = deps.progress.ui;
  const ReaderMode reader_mode = ui.mode;
  const TxtReaderState &txt_reader = ui.txt_reader;
  const int actual_pct = CurrentReaderProgressPercent(deps.progress);
  const int txt_layout_pct = CurrentTxtLayoutProgressPercent(ui);
  const bool txt_progress_computing = (reader_mode == ReaderMode::Txt && txt_reader.open && txt_reader.loading);
  const int pct = txt_progress_computing
                      ? txt_layout_pct
                      : (ui.progress_overlay_dirty ? ClampIntLocal(ui.progress_overlay_preview_pct, 0, 100)
                                                   : actual_pct);
  const int panel_h = scale_px(58);
  const int panel_y = deps.layout.screen_h - panel_h - deps.layout.panel_margin_bottom;
  deps.draw_rect(deps.layout.panel_margin_x,
                 panel_y,
                 deps.layout.screen_w - deps.layout.panel_margin_x * 2,
                 panel_h,
                 SDL_Color{0, 0, 0, 178},
                 true);

  const int bar_x = deps.layout.bar_margin_x;
  const int bar_y = panel_y + scale_px(30);
  const int bar_w = deps.layout.screen_w - deps.layout.bar_margin_x * 2;
  const int bar_h = scale_px(12);
  deps.draw_rect(bar_x, bar_y, bar_w, bar_h, SDL_Color{60, 60, 60, 220}, true);
  const int actual_fill_source = txt_progress_computing ? txt_layout_pct : actual_pct;
  const int actual_fill_w = std::max(0, std::min(bar_w, (bar_w * actual_fill_source) / 100));
  if (actual_fill_w > 0) {
    deps.draw_rect(bar_x, bar_y, actual_fill_w, bar_h, SDL_Color{125, 125, 125, 215}, true);
  }
  const int fill_w = std::max(0, std::min(bar_w, (bar_w * pct) / 100));
  if (fill_w > 0) {
    deps.draw_rect(bar_x, bar_y, fill_w, bar_h, SDL_Color{230, 230, 230, 235}, true);
  }
  deps.draw_rect(bar_x, bar_y, bar_w, bar_h, SDL_Color{255, 255, 255, 220}, false);

  SDL_Color tc{245, 245, 245, 255};
  const std::string pct_text = (pct < 10) ? ("0" + std::to_string(pct)) : std::to_string(pct);
  const bool render_pending =
      (reader_mode == ReaderMode::Pdf && deps.progress.pdf_runtime.IsRenderPending()) ||
      (reader_mode == ReaderMode::Epub && deps.progress.epub_runtime.IsRenderPending()) ||
      (reader_mode == ReaderMode::ZipImage && deps.progress.zip_image_runtime.IsRenderPending());
  const std::string percent =
      txt_progress_computing ? ("(Calculating " + pct_text + "%)")
                             : (render_pending ? ("(Rendering) " + std::to_string(pct) + "%")
                                               : (std::to_string(pct) + "%"));
  TextCacheEntry *te = deps.get_text_texture ? deps.get_text_texture(percent, tc) : nullptr;
  if (te && te->texture) {
    SDL_Rect td{deps.layout.screen_w - deps.layout.percent_margin_x - te->w,
                panel_y + scale_px(8),
                te->w,
                te->h};
    SDL_RenderCopy(deps.renderer, te->texture, nullptr, &td);
  }
#else
  (void)deps;
#endif
}

