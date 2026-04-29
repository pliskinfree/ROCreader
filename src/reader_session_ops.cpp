#include "reader_session_ops.h"

#include <algorithm>
#include "filesystem_compat.h"
#include <iostream>
#include <limits>

namespace {
ReaderOpenRequest MakeOpenRequest(const std::string &book_path, const ReaderOpenDeps &deps) {
  ReaderOpenRequest request;
  request.renderer = deps.renderer;
  request.path = book_path;
  request.screen_w = deps.screen_w;
  request.screen_h = deps.screen_h;
  request.progress = deps.ui.progress;
  request.flow_base_font_pt = deps.epub_flow_base_font_pt ? deps.epub_flow_base_font_pt() : 18;
  request.flow_background_color = deps.epub_flow_background_color ? deps.epub_flow_background_color()
                                                                  : SDL_Color{250, 249, 244, 255};
  request.flow_font_color = deps.epub_flow_font_color ? deps.epub_flow_font_color()
                                                      : SDL_Color{28, 28, 28, 255};
  return request;
}

bool OpenManagedReader(ReaderMode mode, const std::string &book_path, ReaderOpenDeps &deps) {
  if (!deps.reader_manager) return false;
  return deps.reader_manager->Open(mode, MakeOpenRequest(book_path, deps));
}

void CloseManagedReader(ReaderMode mode, ReaderCloseDeps &deps) {
  if (deps.reader_manager) {
    deps.reader_manager->Close(mode);
    return;
  }
  if (mode == ReaderMode::Pdf) {
    deps.pdf_runtime.Close();
  } else if (mode == ReaderMode::Epub) {
    deps.epub_runtime.Close();
  } else if (mode == ReaderMode::ZipImage) {
    deps.zip_image_runtime.Close();
  } else if (mode == ReaderMode::Txt) {
    deps.close_text_reader();
  }
}
}  // namespace

bool OpenReaderSession(const std::string &book_path, const std::string &ext, ReaderOpenDeps &deps) {
  deps.ui.current_book = book_path;
  bool opened = false;

  if (ext == ".txt") {
    opened = OpenManagedReader(ReaderMode::Txt, book_path, deps);
    if (!opened && !deps.reader_manager) {
      opened = deps.open_text_book(book_path);
    }
    if (opened) {
      deps.pdf_runtime.Close();
      deps.epub_runtime.Close();
      deps.zip_image_runtime.Close();
      deps.ui.mode = ReaderMode::Txt;
    }
  } else if (ext == ".pdf") {
    if (OpenManagedReader(ReaderMode::Pdf, book_path, deps)) {
      deps.close_text_reader();
      deps.epub_runtime.Close();
      deps.zip_image_runtime.Close();
      deps.ui.mode = ReaderMode::Pdf;
      opened = true;
    }
    if (!opened && !deps.pdf_runtime.HasRealRenderer()) {
      if (!deps.ui.warned_mock_pdf_backend) {
        std::cerr << "[reader] blocked: current build has no real document backend. "
                     "Please rebuild with REQUIRE_MUPDF=1 and install MuPDF (preferred) or poppler-cpp.\n";
        deps.ui.warned_mock_pdf_backend = true;
      }
    }
  } else if (ext == ".epub") {
    if (deps.file_exists && !deps.file_exists(book_path)) {
      std::cerr << "[reader][epub] file does not exist before open: " << book_path << "\n";
    }
    if (OpenManagedReader(ReaderMode::Epub, book_path, deps)) {
      deps.close_text_reader();
      deps.pdf_runtime.Close();
      deps.zip_image_runtime.Close();
      deps.ui.mode = ReaderMode::Epub;
      opened = true;
    }
    const IReaderModule *epub_module = deps.reader_manager ? deps.reader_manager->Module(ReaderMode::Epub) : nullptr;
    if (!opened && !deps.reader_manager && !deps.epub_runtime.HasRealRenderer()) {
      if (!deps.ui.warned_epub_backend) {
        std::cerr << "[reader] blocked: current build has no epub comic backend. "
                     "Please rebuild with libzip (pkg-config libzip) available.\n";
        deps.ui.warned_epub_backend = true;
      }
    }
    if (!opened) {
      std::cerr << "[reader][epub] runtime open failed backend="
                << (epub_module ? epub_module->BackendName() : deps.epub_runtime.BackendName())
                << " path=" << book_path << "\n";
    }
  } else if (ext == ".zip" || ext == ".cbz") {
    if (deps.file_exists && !deps.file_exists(book_path)) {
      std::cerr << "[reader][zip_image] file does not exist before open: " << book_path << "\n";
    }
    if (OpenManagedReader(ReaderMode::ZipImage, book_path, deps)) {
      deps.close_text_reader();
      deps.pdf_runtime.Close();
      deps.epub_runtime.Close();
      deps.ui.mode = ReaderMode::ZipImage;
      opened = true;
    }
    if (!opened && !deps.zip_image_runtime.HasRealRenderer()) {
      if (!deps.ui.warned_epub_backend) {
        std::cerr << "[reader] blocked: current build has no zip image backend. "
                     "Please rebuild with libzip (pkg-config libzip) available.\n";
        deps.ui.warned_epub_backend = true;
      }
    }
    if (!opened) {
      std::cerr << "[reader][zip_image] runtime open failed backend="
                << deps.zip_image_runtime.BackendName() << " path=" << book_path << "\n";
    }
  } else {
    std::cerr << "[reader] unsupported format for runtime reader: " << book_path << "\n";
  }

  if (!opened) {
    if (ext == ".pdf" || ext == ".epub" || ext == ".zip" || ext == ".cbz") {
      std::cerr << "[reader] failed to open: " << book_path << "\n";
    }
    deps.ui.current_book.clear();
    if (deps.reader_manager) {
      deps.reader_manager->Close(ReaderMode::Txt);
      deps.reader_manager->Close(ReaderMode::Pdf);
      deps.reader_manager->Close(ReaderMode::Epub);
      deps.reader_manager->Close(ReaderMode::ZipImage);
    } else {
      deps.close_text_reader();
      deps.pdf_runtime.Close();
      deps.epub_runtime.Close();
      deps.zip_image_runtime.Close();
    }
  }

  deps.ui.progress_overlay_visible = false;
  ResetReaderInputState(deps.ui);
  return opened;
}

void CloseReaderSession(ReaderCloseDeps &deps) {
  if (deps.ui.mode == ReaderMode::Pdf && deps.pdf_runtime.IsOpen()) {
    const PdfRuntimeProgress active_pdf = deps.pdf_runtime.Progress();
    deps.ui.progress.page = active_pdf.page;
    deps.ui.progress.scroll_x = active_pdf.scroll_x;
    deps.ui.progress.scroll_y = active_pdf.scroll_y;
    deps.ui.progress.zoom = active_pdf.zoom;
    deps.ui.progress.rotation = active_pdf.rotation;
  } else if (deps.ui.mode == ReaderMode::Epub) {
    const IReaderModule *epub_module = deps.reader_manager ? deps.reader_manager->Module(ReaderMode::Epub) : nullptr;
    if (epub_module && epub_module->IsOpen()) {
      const ReaderProgress active_epub = epub_module->Progress();
      deps.ui.progress.page = active_epub.page;
      deps.ui.progress.scroll_x =
          std::string(epub_module->BackendName()) == "epub-flow" ? 0 : active_epub.scroll_x;
      deps.ui.progress.scroll_y = active_epub.scroll_y;
      deps.ui.progress.zoom = active_epub.zoom;
      deps.ui.progress.rotation = active_epub.rotation;
    } else if (!deps.reader_manager && deps.epub_runtime.IsOpen()) {
      const EpubRuntimeProgress active_epub = deps.epub_runtime.Progress();
      deps.ui.progress.page = active_epub.page;
      deps.ui.progress.scroll_x =
          std::string(deps.epub_runtime.BackendName()) == "epub-flow" ? 0 : active_epub.scroll_x;
      deps.ui.progress.scroll_y = active_epub.scroll_y;
      deps.ui.progress.zoom = active_epub.zoom;
      deps.ui.progress.rotation = active_epub.rotation;
    }
  } else if (deps.ui.mode == ReaderMode::ZipImage && deps.zip_image_runtime.IsOpen()) {
    const ZipImageRuntimeProgress active_zip = deps.zip_image_runtime.Progress();
    deps.ui.progress.page = active_zip.page;
    deps.ui.progress.scroll_x = active_zip.scroll_x;
    deps.ui.progress.scroll_y = active_zip.scroll_y;
    deps.ui.progress.zoom = active_zip.zoom;
    deps.ui.progress.rotation = active_zip.rotation;
  } else if (deps.ui.mode == ReaderMode::Txt && deps.ui.Txt().open) {
    if (!deps.ui.Txt().line_source_offsets.empty()) {
      const size_t top_line = std::min(
          deps.ui.Txt().line_source_offsets.size() - 1,
          static_cast<size_t>(std::max(0, deps.ui.Txt().scroll_px /
                                                 std::max(1, deps.ui.Txt().line_h))));
      deps.ui.progress.scroll_x = static_cast<int>(std::min<size_t>(
          deps.ui.Txt().line_source_offsets[top_line], static_cast<size_t>(std::numeric_limits<int>::max())));
    } else {
      deps.ui.progress.scroll_x = 0;
    }
    deps.ui.progress.page = (deps.ui.Txt().line_h > 0) ? (deps.ui.Txt().scroll_px / deps.ui.Txt().line_h) : 0;
    deps.ui.progress.scroll_y = deps.ui.Txt().scroll_px;
    deps.ui.Txt().resume_cache_dirty = true;
    deps.persist_current_txt_resume_snapshot(deps.ui.current_book, true);
  }

  deps.progress_store.Set(deps.ui.current_book, deps.ui.progress);

  if (deps.ui.mode == ReaderMode::Pdf) {
    CloseManagedReader(ReaderMode::Pdf, deps);
  } else if (deps.ui.mode == ReaderMode::Epub) {
    CloseManagedReader(ReaderMode::Epub, deps);
  } else if (deps.ui.mode == ReaderMode::ZipImage) {
    CloseManagedReader(ReaderMode::ZipImage, deps);
  } else if (deps.ui.mode == ReaderMode::Txt) {
    CloseManagedReader(ReaderMode::Txt, deps);
  }

  deps.ui.mode = ReaderMode::None;
  deps.ui.progress_overlay_visible = false;
  ResetReaderInputState(deps.ui);
}
