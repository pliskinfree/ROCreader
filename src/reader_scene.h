#pragma once

#include "input_manager.h"
#include "reader_input_router.h"
#include "reader_manager.h"
#include "reader_progress_controller.h"
#include "reader_session_ops.h"
#include "reader_session_state.h"
#include "txt_reader_runtime.h"
#include "ui_text_cache.h"

#include <SDL.h>

#include <functional>
#include <string>

struct LayoutMetrics;

struct ReaderSceneProgressInputConfig {
  int tap_step_px = 0;
  int overlay_tap_step_pct = 1;
  float hold_delay_sec = 0.0f;
  float hold_speed_min_pct = 0.0f;
  float hold_speed_max_pct = 0.0f;
  float hold_accel_pct = 0.0f;
};

ReaderSceneProgressInputConfig MakeReaderSceneProgressInputConfig(int tap_step_px);

struct ReaderSceneInputActions {
  std::function<int()> current_progress_pct;
  std::function<void(int)> jump_to_percent;
  std::function<void(size_t)> jump_to_txt_source_offset;
  std::function<void(int)> text_scroll_by;
  std::function<void(int)> text_page_by;
  std::function<void(const std::string &, uint32_t, bool)> show_transient_message;
};

struct ReaderSceneInputServices {
  std::function<void()> close_text_reader;
  std::function<void(const std::string &, bool)> persist_current_txt_resume_snapshot;
  ReaderSceneInputActions actions;
};

struct ReaderSceneInputDeps {
  const InputManager &input;
  ReaderUiState &ui;
  ProgressStore &progress;
  ReaderManager *reader_manager = nullptr;
  PdfRuntime &pdf_runtime;
  EpubRuntime &epub_runtime;
  ZipImageRuntime &zip_image_runtime;
  float dt = 0.0f;
  ReaderSceneProgressInputConfig progress_input;
  bool rgds_mode = false;
  bool &transient_message_dismissed_this_frame;
  ReaderSceneInputServices services;
};

struct ReaderSceneProgressOverlayMetrics {
  int panel_margin_x = 0;
  int panel_margin_bottom = 0;
  int bar_margin_x = 0;
  int percent_margin_x = 0;
};

ReaderSceneProgressOverlayMetrics MakeReaderSceneProgressOverlayMetrics(const LayoutMetrics &layout);

struct ReaderSceneRenderServices {
  std::function<int(int)> scale_px;
  std::function<void(int, int, int, int, SDL_Color, bool)> draw_rect;
  std::function<void()> clamp_text_scroll;
  std::function<void(const SDL_Rect &)> set_clip_rect;
  std::function<void()> clear_clip_rect;
  std::function<TextCacheEntry *(const std::string &, SDL_Color)> get_text_texture;
  std::function<TextCacheEntry *(const std::string &, SDL_Color)> get_reader_text_texture;
};

ReaderSceneInputServices MakeReaderSceneInputServices(
    std::function<void()> close_text_reader,
    std::function<void(const std::string &, bool)> persist_current_txt_resume_snapshot,
    ReaderSceneInputActions actions);

ReaderSceneRenderServices MakeReaderSceneRenderServices(
    SDL_Renderer *renderer,
    std::function<int(int)> scale_px,
    std::function<void(int, int, int, int, SDL_Color, bool)> draw_rect,
    std::function<void()> clamp_text_scroll,
    std::function<TextCacheEntry *(const std::string &, SDL_Color)> get_text_texture,
    std::function<TextCacheEntry *(const std::string &, SDL_Color)> get_reader_text_texture);

struct ReaderSceneRenderDeps {
  SDL_Renderer *renderer = nullptr;
  ReaderUiState &ui;
  ReaderManager *reader_manager = nullptr;
  ReaderProgressControllerDeps &progress;
  float dt = 0.0f;
  int screen_w = 0;
  int screen_h = 0;
  SDL_Color txt_background_color{0, 0, 0, 255};
  SDL_Color txt_font_color{255, 255, 255, 255};
  int chapter_sidebar_w = 0;
  ReaderSceneProgressOverlayMetrics progress_overlay_metrics;
  SDL_Rect overlay_viewport{0, 0, 0, 0};
  bool overlay_viewport_enabled = false;
  bool rgds_mode = false;
  ReaderSceneRenderServices services;
  bool tick_modules = true;
};

class ReaderScene {
public:
  explicit ReaderScene(std::function<void()> on_reader_closed = {});

  bool IsRenderPending(const ReaderUiState &ui, const ReaderManager *reader_manager) const;
  void HandleInput(const ReaderSceneInputDeps &deps) const;
  void Draw(const ReaderSceneRenderDeps &deps) const;

private:
  void OnReaderClosed() const;

  std::function<void()> on_reader_closed_;
};
