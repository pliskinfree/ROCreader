#pragma once

#include "epub_runtime.h"
#include "pdf_runtime.h"
#include "reader_manager.h"
#include "reader_session_state.h"
#include "ui_text_cache.h"
#include "zip_image_runtime.h"

#include <SDL.h>

#include <functional>
#include <string>

struct ReaderProgressControllerDeps {
  ReaderUiState &ui;
  PdfRuntime &pdf_runtime;
  EpubRuntime &epub_runtime;
  ZipImageRuntime &zip_image_runtime;
  ReaderManager *reader_manager = nullptr;
  std::function<void(int)> text_jump_to_percent;
};

int ReaderPageProgressPercent(int page_index, int page_count);
int ReaderPageIndexForPercent(int current_page, int target_pct, int page_count);
void ReaderJumpToPercent(ReaderProgressControllerDeps &deps, int pct);
int CurrentReaderProgressPercent(const ReaderProgressControllerDeps &deps);
int CurrentTxtLayoutProgressPercent(const ReaderUiState &ui);

struct ReaderProgressOverlayLayout {
  int screen_w = 0;
  int screen_h = 0;
  int panel_margin_x = 0;
  int panel_margin_bottom = 0;
  int bar_margin_x = 0;
  int percent_margin_x = 0;
};

struct ReaderProgressOverlayRenderDeps {
  SDL_Renderer *renderer = nullptr;
  ReaderProgressControllerDeps &progress;
  ReaderProgressOverlayLayout layout;
  std::function<int(int)> scale_px;
  std::function<void(int, int, int, int, SDL_Color, bool)> draw_rect;
  std::function<TextCacheEntry *(const std::string &, SDL_Color)> get_text_texture;
  int origin_x = 0;
  int origin_y = 0;
};

void DrawReaderProgressOverlay(ReaderProgressOverlayRenderDeps &deps);
