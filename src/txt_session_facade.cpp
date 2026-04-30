#include "txt_session_facade.h"

#include "epub_reader.h"
#include "runtime_log.h"

#ifdef HAVE_SDL2_TTF
#include <SDL_ttf.h>
#endif

#include <algorithm>
#include <iostream>
#include <limits>

TxtSessionFacade::TxtSessionFacade(TxtSessionFacadeDeps deps) : deps_(std::move(deps)) {}

TxtReaderSessionDeps TxtSessionFacade::MakeDeps() const {
  return TxtReaderSessionDeps{
      deps_.ui,
      deps_.text_service.layout_cache,
      deps_.open_ui_font,
      deps_.has_reader_font,
      deps_.reader_font_height,
      deps_.get_text_viewport_bounds,
      [&](const std::string &path, const SDL_Rect &bounds, int line_h, uintmax_t file_size, long long file_mtime) {
        return MakeTxtLayoutCacheKey(path, bounds, line_h, file_size, file_mtime, deps_.normalize_path_key) +
               "|v" + std::to_string(deps_.txt_layout_cache_version) +
               "|bytes=" + std::to_string(deps_.txt_max_bytes) +
               "|lines=" + std::to_string(deps_.txt_max_wrapped_lines);
      },
      [&](const std::string &cache_key, const std::string &book_path, TxtLayoutCacheEntry &entry) {
        return LoadTxtLayoutCacheFromDisk(deps_.text_service, cache_key, book_path, entry);
      },
      [&](const std::string &cache_key, const std::string &book_path, const TxtLayoutCacheEntry &entry) {
        SaveTxtLayoutCacheToDisk(deps_.text_service, cache_key, book_path, entry);
      },
      [&](const std::string &cache_key, const std::string &book_path, TxtResumeCacheEntry &entry) {
        return LoadTxtResumeCacheFromDisk(deps_.text_service, cache_key, book_path, entry);
      },
      [&](const std::string &cache_key, const std::string &book_path, const TxtReaderState &state) {
        SaveTxtResumeCacheToDisk(deps_.text_service, cache_key, book_path, state);
      },
      [&]() { PruneTxtLayoutCache(deps_.text_service); },
      deps_.decode_text_bytes_to_utf8,
      deps_.append_wrapped_text_line,
      deps_.invalidate_all_render_cache,
      deps_.clamp_text_scroll,
      deps_.txt_line_spacing,
      deps_.txt_max_bytes,
      deps_.txt_resume_save_delay_ms,
  };
}

TxtReaderModuleCallbacks TxtSessionFacade::ModuleCallbacks(const std::string &current_book) const {
  return TxtReaderModuleCallbacks{
      [this](const std::string &path) { return OpenTextBook(path); },
      [this]() { Close(); },
      [this, &current_book](int delta_px) { ScrollBy(current_book, delta_px); },
      [this, &current_book](int dir) { PageBy(current_book, dir); },
      [this, &current_book](int pct) { JumpToPercent(current_book, pct); },
  };
}

void TxtSessionFacade::Close() const {
  deps_.ui.Txt() = TxtReaderState{};
  deps_.ui.progress_overlay_visible = false;
  if (deps_.ui.mode == ReaderMode::Txt) {
    deps_.ui.mode = ReaderMode::None;
  }
}

bool TxtSessionFacade::OpenTextBook(const std::string &path) const {
#ifndef HAVE_SDL2_TTF
  (void)path;
  std::cerr << "[reader] txt reader requires SDL_ttf build support.\n";
  return false;
#else
  auto session_deps = MakeDeps();
  return OpenTextBookSession(path, session_deps);
#endif
}

bool TxtSessionFacade::OpenEpubTextFallback(const std::string &path) const {
#ifndef HAVE_SDL2_TTF
  (void)path;
  return false;
#else
  EpubReader epub_reader;
  EpubReader::ExtractedText extracted;
  std::string error;
  if (!epub_reader.ExtractText(path, deps_.epub_asset_cache_dir.string(), extracted, error)) {
    runtime_log::Line("[reader][epub] text fallback failed path=" + path + " error=" + error);
    std::cerr << "[reader][epub] text fallback failed: " << error << " path=" << path << "\n";
    return false;
  }
  auto session_deps = MakeDeps();
  const bool opened = OpenTextBufferSession(path, std::move(extracted.text), extracted.logical_size,
                                            extracted.logical_mtime, session_deps);
  runtime_log::Line(std::string("[reader][epub] text fallback ") +
                    (opened ? "opened " : "failed ") + path);
  return opened;
#endif
}

void TxtSessionFacade::TickLoading(const std::string &book_path, uint32_t budget_ms, size_t byte_budget) const {
  auto session_deps = MakeDeps();
  TickTextBookSession(book_path, session_deps, budget_ms, byte_budget);
}

void TxtSessionFacade::PersistResumeSnapshot(const std::string &book_path, bool force) const {
  auto session_deps = MakeDeps();
  PersistCurrentTxtResumeSnapshot(book_path, force, session_deps);
}

void TxtSessionFacade::ScrollBy(const std::string &book_path, int delta_px) const {
  auto session_deps = MakeDeps();
  TextScrollBy(delta_px, book_path, session_deps);
}

void TxtSessionFacade::PageBy(const std::string &book_path, int dir) const {
  auto session_deps = MakeDeps();
  TextPageBy(dir, book_path, session_deps);
}

void TxtSessionFacade::JumpToPercent(const std::string &book_path, int pct) const {
  auto session_deps = MakeDeps();
  TextJumpToPercent(pct, book_path, session_deps);
}

void TxtSessionFacade::JumpToSourceOffset(const std::string &book_path, size_t source_offset) const {
  if (!(deps_.ui.mode == ReaderMode::Txt && deps_.ui.Txt().open)) return;
  TxtReaderState &txt = deps_.ui.Txt();
  txt.restore_source_offset = source_offset;
  auto session_deps = MakeDeps();
  if (txt.loading) {
    WarmTextReaderToTarget(txt, &txt.cache_key, session_deps);
  }
  if (!txt.line_source_offsets.empty()) {
    auto upper = std::upper_bound(txt.line_source_offsets.begin(), txt.line_source_offsets.end(), source_offset);
    size_t line_index = 0;
    if (upper != txt.line_source_offsets.begin()) {
      line_index = static_cast<size_t>(std::distance(txt.line_source_offsets.begin(), upper - 1));
    }
    const int max_scroll = std::max(0, txt.content_h - txt.viewport_h);
    const size_t raw_scroll = line_index * static_cast<size_t>(std::max(1, txt.line_h));
    const int scroll_px = static_cast<int>(
        std::min<size_t>(raw_scroll, static_cast<size_t>(std::numeric_limits<int>::max())));
    txt.target_scroll_px = std::clamp(scroll_px, 0, max_scroll);
    txt.scroll_px = txt.target_scroll_px;
  }
  deps_.clamp_text_scroll();
  txt.resume_cache_dirty = true;
  PersistResumeSnapshot(book_path, true);
}
