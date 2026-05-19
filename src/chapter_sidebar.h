#pragma once

#include "input_manager.h"
#include "reader_session_state.h"
#include "ui_text_cache.h"

#include <SDL.h>

#include <functional>
#include <string>

struct ChapterSidebarInputDeps {
  const InputManager &input;
  ReaderUiState &ui;
  std::function<void(const ReaderChapterAnchor &)> jump_to_chapter;
};

struct ChapterSidebarRenderDeps {
  SDL_Renderer *renderer = nullptr;
  ReaderUiState &ui;
  int screen_w = 0;
  int screen_h = 0;
  int sidebar_w = 0;
  int origin_x = 0;
  int origin_y = 0;
  std::function<int(int)> scale_px;
  std::function<void(int, int, int, int, SDL_Color, bool)> draw_rect;
  std::function<TextCacheEntry *(const std::string &, SDL_Color)> get_text_texture;
};

bool HandleChapterSidebarInput(const ChapterSidebarInputDeps &deps);
void DrawChapterSidebar(const ChapterSidebarRenderDeps &deps);
