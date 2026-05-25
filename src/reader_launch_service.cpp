#include "reader_launch_service.h"

#include "epub_runtime.h"
#include "pdf_runtime.h"
#include "zip_image_runtime.h"

ReaderLaunchService::ReaderLaunchService(ReaderLaunchServiceDeps deps) : deps_(std::move(deps)) {}

bool ReaderLaunchService::OpenFromShelfItem(const BookItem &item) const {
  const std::string &real_path = deps_.item_real_path(item);
  const std::string ext = deps_.get_lower_ext(real_path);
  const std::string open_path = real_path;

  deps_.open_deps.ui.progress = deps_.get_compatible_progress(item);
  bool final_opened = OpenReaderSession(open_path, ext, deps_.open_deps);

  if (!final_opened && ext == ".epub") {
    deps_.open_deps.ui.current_book = open_path;
    final_opened = deps_.open_epub_text_book && deps_.open_epub_text_book(open_path);
    if (final_opened) {
      if (deps_.open_deps.runtimes.epub_runtime) deps_.open_deps.runtimes.epub_runtime->Close();
      if (deps_.open_deps.runtimes.pdf_runtime) deps_.open_deps.runtimes.pdf_runtime->Close();
      if (deps_.open_deps.runtimes.zip_image_runtime) deps_.open_deps.runtimes.zip_image_runtime->Close();
      deps_.open_deps.ui.mode = ReaderMode::Txt;
    }
  }

  if (final_opened) {
    deps_.add_history(open_path);
    deps_.open_deps.ui.current_book = open_path;
    deps_.enter_reader_scene();
    deps_.start_scene_flash();
  } else {
    deps_.show_open_failed("Open failed");
    deps_.enter_shelf_scene();
  }

  return final_opened;
}

std::function<bool(const BookItem &)> MakeShelfReaderLaunchHandler(ShelfReaderLaunchHandlerDeps deps) {
  return [deps](const BookItem &item) mutable {
    ReaderOpenDeps open_deps{
        ReaderRenderEnv{
            deps.renderer,
            deps.screen_w ? deps.screen_w() : 0,
            deps.screen_h ? deps.screen_h() : 0,
        },
        deps.ui,
        ReaderFormatRuntimes{
            deps.reader_manager,
            deps.pdf_runtime,
            deps.epub_runtime,
            deps.zip_image_runtime,
        },
        ReaderFlowStyle{
            deps.epub_flow_base_font_pt,
            deps.epub_flow_background_color,
            deps.epub_flow_font_color,
        },
        ReaderSessionCallbacks{
            deps.open_text_book,
            deps.close_text_reader,
            deps.file_exists,
        },
    };
    ReaderLaunchService reader_launch{
        ReaderLaunchServiceDeps{
            open_deps,
            deps.item_real_path,
            deps.get_compatible_progress,
            deps.get_lower_ext,
            deps.open_epub_text_book,
            deps.add_history,
            deps.enter_reader_scene,
            deps.enter_shelf_scene,
            deps.start_scene_flash,
            deps.show_open_failed,
        },
    };
    return reader_launch.OpenFromShelfItem(item);
  };
}
