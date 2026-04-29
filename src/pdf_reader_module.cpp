#include "pdf_reader_module.h"

PdfReaderModule::PdfReaderModule(PdfRuntime &runtime) : runtime_(runtime) {}

bool PdfReaderModule::Open(const ReaderOpenRequest &request) {
  PdfRuntimeProgress progress;
  progress.page = request.progress.page;
  progress.rotation = request.progress.rotation;
  progress.zoom = request.progress.zoom;
  progress.scroll_x = request.progress.scroll_x;
  progress.scroll_y = request.progress.scroll_y;
  restore_progress_ = request.progress;
  return runtime_.Open(request.renderer, request.path, request.screen_w, request.screen_h, progress);
}

void PdfReaderModule::Close() {
  runtime_.Close();
}

bool PdfReaderModule::IsOpen() const {
  return runtime_.IsOpen();
}

void PdfReaderModule::UpdateViewport(int w, int h) {
  runtime_.UpdateViewport(w, h);
}

void PdfReaderModule::Tick(float dt) {
  (void)dt;
  runtime_.Tick();
}

void PdfReaderModule::Draw(SDL_Renderer *renderer) {
  runtime_.Draw(renderer);
}

void PdfReaderModule::HandleInput(const InputManager &input, float dt) {
  (void)input;
  (void)dt;
}

ReaderProgress PdfReaderModule::Progress() const {
  const PdfRuntimeProgress progress = runtime_.Progress();
  return ReaderProgress{progress.page, progress.rotation, progress.zoom, progress.scroll_x, progress.scroll_y};
}

void PdfReaderModule::RestoreProgress(const ReaderProgress &progress) {
  restore_progress_ = progress;
}

int PdfReaderModule::PageCount() const {
  return runtime_.PageCount();
}

int PdfReaderModule::CurrentPage() const {
  return runtime_.CurrentPage();
}

ReaderCapabilities PdfReaderModule::Capabilities() const {
  ReaderCapabilities capabilities;
  capabilities.supports_rotation = true;
  capabilities.supports_zoom = true;
  capabilities.supports_horizontal_pan = true;
  capabilities.supports_continuous_scroll = true;
  return capabilities;
}

