#include "txt_reader_module.h"

#include "txt_reader_session.h"

#include <algorithm>
#include <limits>
#include <utility>

TxtReaderModule::TxtReaderModule(ReaderUiState &ui, TxtReaderModuleCallbacks callbacks)
    : ui_(ui), callbacks_(std::move(callbacks)) {}

bool TxtReaderModule::Open(const ReaderOpenRequest &request) {
  return callbacks_.open_text_book ? callbacks_.open_text_book(request.path) : false;
}

void TxtReaderModule::Close() {
  if (callbacks_.close_text_reader) callbacks_.close_text_reader();
}

bool TxtReaderModule::IsOpen() const {
  return ui_.mode == ReaderMode::Txt && ui_.txt_reader.open;
}

void TxtReaderModule::UpdateViewport(int w, int h) {
  ui_.txt_reader.viewport_w = w;
  ui_.txt_reader.viewport_h = h;
}

void TxtReaderModule::Tick(float dt) {
  (void)dt;
}

void TxtReaderModule::Draw(SDL_Renderer *renderer) {
  (void)renderer;
}

void TxtReaderModule::HandleInput(const InputManager &input, float dt) {
  (void)input;
  (void)dt;
}

ReaderProgress TxtReaderModule::Progress() const {
  ReaderProgress progress = ui_.progress;
  if (!ui_.txt_reader.line_source_offsets.empty()) {
    const size_t top_line = std::min(
        ui_.txt_reader.line_source_offsets.size() - 1,
        static_cast<size_t>(std::max(0, ui_.txt_reader.scroll_px / std::max(1, ui_.txt_reader.line_h))));
    progress.scroll_x = static_cast<int>(std::min<size_t>(
        ui_.txt_reader.line_source_offsets[top_line], static_cast<size_t>(std::numeric_limits<int>::max())));
  } else {
    progress.scroll_x = 0;
  }
  progress.page = (ui_.txt_reader.line_h > 0) ? (ui_.txt_reader.scroll_px / ui_.txt_reader.line_h) : 0;
  progress.scroll_y = ui_.txt_reader.scroll_px;
  return progress;
}

void TxtReaderModule::RestoreProgress(const ReaderProgress &progress) {
  ui_.progress = progress;
}

int TxtReaderModule::PageCount() const {
  const int viewport_h = std::max(1, ui_.txt_reader.viewport_h);
  return std::max(1, (ui_.txt_reader.content_h + viewport_h - 1) / viewport_h);
}

int TxtReaderModule::CurrentPage() const {
  const int viewport_h = std::max(1, ui_.txt_reader.viewport_h);
  return std::max(0, ui_.txt_reader.scroll_px / viewport_h);
}

ReaderCapabilities TxtReaderModule::Capabilities() const {
  ReaderCapabilities capabilities;
  capabilities.supports_continuous_scroll = true;
  capabilities.uses_txt_theme = true;
  capabilities.is_flow_layout = true;
  return capabilities;
}
