#pragma once

#include "progress_store.h"
#include "reader_manager.h"
#include "reader_session_state.h"

#include <SDL.h>

#include <functional>
#include <string>

class EpubRuntime;
class PdfRuntime;
class ZipImageRuntime;

struct ReaderRenderEnv {
  SDL_Renderer *renderer = nullptr;
  int screen_w = 0;
  int screen_h = 0;
};

struct ReaderFormatRuntimes {
  ReaderManager *reader_manager = nullptr;
  PdfRuntime *pdf_runtime = nullptr;
  EpubRuntime *epub_runtime = nullptr;
  ZipImageRuntime *zip_image_runtime = nullptr;
};

struct ReaderFlowStyle {
  std::function<int()> epub_flow_base_font_pt;
  std::function<SDL_Color()> epub_flow_background_color;
  std::function<SDL_Color()> epub_flow_font_color;
};

struct ReaderSessionCallbacks {
  std::function<bool(const std::string &)> open_text_book;
  std::function<void()> close_text_reader;
  std::function<bool(const std::string &)> file_exists;
};

struct ReaderCloseCallbacks {
  std::function<void()> close_text_reader;
  std::function<void(const std::string &, bool)> persist_current_txt_resume_snapshot;
};

struct ReaderOpenDeps {
  ReaderRenderEnv render;
  ReaderUiState &ui;
  ReaderFormatRuntimes runtimes;
  ReaderFlowStyle flow_style;
  ReaderSessionCallbacks callbacks;
};

struct ReaderCloseDeps {
  ReaderUiState &ui;
  ProgressStore &progress_store;
  ReaderFormatRuntimes runtimes;
  ReaderCloseCallbacks callbacks;
};

bool OpenReaderSession(const std::string &book_path, const std::string &ext, ReaderOpenDeps &deps);
void CloseReaderSession(ReaderCloseDeps &deps);
