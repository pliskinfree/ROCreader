#pragma once

#include "epub_runtime.h"
#include "input_manager.h"
#include "pdf_runtime.h"
#include "reader_manager.h"
#include "reader_session_state.h"
#include "txt_reader_runtime.h"
#include "zip_image_runtime.h"

#include <array>
#include <functional>
#include <string>

struct ReaderInputRouterDeps {
  const InputManager &input;
  ReaderUiState &ui;
  PdfRuntime &pdf_runtime;
  EpubRuntime &epub_runtime;
  ZipImageRuntime &zip_image_runtime;
  ReaderManager *reader_manager = nullptr;
  float dt = 0.0f;
  int tap_step_px = 0;
  int progress_overlay_tap_step_pct = 1;
  float progress_overlay_hold_delay_sec = 0.0f;
  float progress_overlay_hold_speed_min_pct = 0.0f;
  float progress_overlay_hold_speed_max_pct = 0.0f;
  float progress_overlay_hold_accel_pct = 0.0f;
  bool &transient_message_dismissed_this_frame;
  std::function<int()> current_reader_progress_pct;
  std::function<void(int)> reader_jump_to_percent;
  std::function<void(int)> text_scroll_by;
  std::function<void(int)> text_page_by;
  std::function<void(const std::string &, uint32_t, bool)> show_transient_message;
};

void HandleReaderInput(ReaderInputRouterDeps &deps);
