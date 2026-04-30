#pragma once

#include "reader_chapter.h"
#include "reader_core.h"

#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct TxtReaderState {
  bool open = false;
  std::vector<std::string> lines;
  std::vector<size_t> line_source_offsets;
  int scroll_px = 0;
  int target_scroll_px = 0;
  int viewport_x = 0;
  int viewport_y = 0;
  int viewport_w = 0;
  int viewport_h = 0;
  int line_h = 28;
  int content_h = 0;
  bool paragraph_spacing_pending = false;
  bool last_raw_line_blank = true;
  std::string pending_raw;
  std::string pending_line;
  size_t pending_line_source_offset = 0;
  std::string cache_key;
  size_t parse_pos = 0;
  size_t layout_total_bytes = 0;
  size_t restore_source_offset = 0;
  bool loading = false;
  bool truncated = false;
  bool limit_hit = false;
  bool truncation_notice_added = false;
  uint32_t last_resume_cache_save = 0;
  bool resume_cache_dirty = false;
};

struct TxtLayoutCacheEntry {
  std::vector<std::string> lines;
  std::vector<size_t> line_source_offsets;
  int viewport_w = 0;
  int viewport_h = 0;
  int line_h = 0;
  int content_h = 0;
  bool truncated = false;
  bool limit_hit = false;
  uint32_t last_use = 0;
};

struct TxtResumeCacheEntry {
  std::vector<std::string> lines;
  std::vector<size_t> line_source_offsets;
  std::string pending_raw;
  std::string pending_line;
  size_t pending_line_source_offset = 0;
  int viewport_w = 0;
  int viewport_h = 0;
  int line_h = 0;
  int content_h = 0;
  bool paragraph_spacing_pending = false;
  bool last_raw_line_blank = true;
  int scroll_px = 0;
  int target_scroll_px = 0;
  size_t parse_pos = 0;
  size_t restore_source_offset = 0;
  bool loading = false;
  bool truncated = false;
  bool limit_hit = false;
  bool truncation_notice_added = false;
};

struct TxtTranscodeJob {
  bool active = false;
  std::vector<std::string> files;
  size_t total = 0;
  size_t processed = 0;
  size_t converted = 0;
  size_t failed = 0;
  std::string current_file;
};

struct TxtChapterScanState {
  std::string cache_key;
  size_t scan_pos = 0;
  size_t last_anchor_line = 0;
  bool have_last_anchor = false;
  bool done = false;
  std::unordered_set<std::string> seen_titles;
  std::vector<ReaderChapterAnchor> anchors;
  uint32_t last_tick_ms = 0;
};

struct ReaderUiState {
  ReaderUiState() : txt_reader_ptr(&txt_reader_storage) {}

  TxtReaderState &Txt() { return *txt_reader_ptr; }
  const TxtReaderState &Txt() const { return *txt_reader_ptr; }
  void BindTxtReaderState(TxtReaderState *state) {
    txt_reader_ptr = state ? state : &txt_reader_storage;
  }

  std::string current_book;
  ReaderProgress progress;
  ReaderMode mode = ReaderMode::None;
  bool progress_overlay_visible = false;
  bool progress_overlay_scrubbing = false;
  bool progress_overlay_dirty = false;
  int progress_overlay_preview_pct = 0;
  float progress_overlay_preview_pct_f = 0.0f;
  bool chapter_sidebar_visible = false;
  int chapter_sidebar_selected = 0;
  int chapter_sidebar_first_visible = 0;
  int chapter_sidebar_page_rows = 1;
  int chapter_marquee_selected = -1;
  uint32_t chapter_marquee_start_ticks = 0;
  bool chapter_loading = false;
  int chapter_loading_pct = 0;
  std::string chapter_cache_key;
  std::vector<ReaderChapterAnchor> chapter_anchors;
  TxtChapterScanState txt_chapter_scan;
  float hold_cooldown = 0.0f;
  std::array<float, kButtonCount> hold_speed{};
  std::array<bool, kButtonCount> long_fired{};
  std::array<float, kButtonCount> progress_overlay_hold_speed{};
  std::array<bool, kButtonCount> progress_overlay_long_fired{};
  bool warned_mock_pdf_backend = false;
  bool warned_epub_backend = false;

private:
  TxtReaderState txt_reader_storage;
  TxtReaderState *txt_reader_ptr = nullptr;
};

inline void ResetReaderInputState(ReaderUiState &state) {
  state.hold_cooldown = 0.0f;
  for (auto &value : state.hold_speed) value = 0.0f;
  for (auto &value : state.long_fired) value = false;
  state.progress_overlay_scrubbing = false;
  state.progress_overlay_dirty = false;
  for (auto &value : state.progress_overlay_hold_speed) value = 0.0f;
  for (auto &value : state.progress_overlay_long_fired) value = false;
}
