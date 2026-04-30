#pragma once

#include "book_scanner.h"
#include "reader_session_ops.h"

#include <functional>
#include <string>

class EpubRuntime;
class PdfRuntime;
class ReaderManager;
class ZipImageRuntime;

struct ReaderLaunchServiceDeps {
  ReaderOpenDeps &open_deps;
  std::function<const std::string &(const BookItem &)> item_real_path;
  std::function<ReaderProgress(const BookItem &)> get_compatible_progress;
  std::function<std::string(const std::string &)> get_lower_ext;
  std::function<bool(const std::string &)> open_epub_text_book;
  std::function<void(const std::string &)> add_history;
  std::function<void()> enter_reader_scene;
  std::function<void()> enter_shelf_scene;
  std::function<void()> start_scene_flash;
  std::function<void(const std::string &)> show_open_failed;
};

class ReaderLaunchService {
public:
  explicit ReaderLaunchService(ReaderLaunchServiceDeps deps);

  bool OpenFromShelfItem(const BookItem &item) const;

private:
  ReaderLaunchServiceDeps deps_;
};

struct ShelfReaderLaunchHandlerDeps {
  SDL_Renderer *renderer = nullptr;
  std::function<int()> screen_w;
  std::function<int()> screen_h;
  ReaderUiState &ui;
  ReaderManager *reader_manager = nullptr;
  PdfRuntime *pdf_runtime = nullptr;
  EpubRuntime *epub_runtime = nullptr;
  ZipImageRuntime *zip_image_runtime = nullptr;
  std::function<int()> epub_flow_base_font_pt;
  std::function<SDL_Color()> epub_flow_background_color;
  std::function<SDL_Color()> epub_flow_font_color;
  std::function<bool(const std::string &)> open_text_book;
  std::function<void()> close_text_reader;
  std::function<bool(const std::string &)> file_exists;
  std::function<const std::string &(const BookItem &)> item_real_path;
  std::function<ReaderProgress(const BookItem &)> get_compatible_progress;
  std::function<std::string(const std::string &)> get_lower_ext;
  std::function<bool(const std::string &)> open_epub_text_book;
  std::function<void(const std::string &)> add_history;
  std::function<void()> enter_reader_scene;
  std::function<void()> enter_shelf_scene;
  std::function<void()> start_scene_flash;
  std::function<void(const std::string &)> show_open_failed;
};

std::function<bool(const BookItem &)> MakeShelfReaderLaunchHandler(ShelfReaderLaunchHandlerDeps deps);
