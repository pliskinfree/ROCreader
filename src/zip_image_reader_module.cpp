#include "zip_image_reader_module.h"

ZipImageReaderModule::ZipImageReaderModule(ZipImageRuntime &runtime) : runtime_(runtime) {}

bool ZipImageReaderModule::Open(const ReaderOpenRequest &request) {
  ZipImageRuntimeProgress progress;
  progress.page = request.progress.page;
  progress.rotation = request.progress.rotation;
  progress.zoom = request.progress.zoom;
  progress.scroll_x = request.progress.scroll_x;
  progress.scroll_y = request.progress.scroll_y;
  restore_progress_ = request.progress;
  return runtime_.Open(request.renderer, request.path, request.screen_w, request.screen_h, progress);
}

void ZipImageReaderModule::Close() {
  runtime_.Close();
}

bool ZipImageReaderModule::IsOpen() const {
  return runtime_.IsOpen();
}

void ZipImageReaderModule::UpdateViewport(int w, int h) {
  runtime_.UpdateViewport(w, h);
}

void ZipImageReaderModule::Tick(float dt) {
  (void)dt;
  runtime_.Tick();
}

void ZipImageReaderModule::Draw(SDL_Renderer *renderer) {
  runtime_.Draw(renderer);
}

void ZipImageReaderModule::HandleInput(const InputManager &input, float dt) {
  (void)input;
  (void)dt;
}

ReaderProgress ZipImageReaderModule::Progress() const {
  const ZipImageRuntimeProgress progress = runtime_.Progress();
  return ReaderProgress{progress.page, progress.rotation, progress.zoom, progress.scroll_x, progress.scroll_y};
}

void ZipImageReaderModule::RestoreProgress(const ReaderProgress &progress) {
  restore_progress_ = progress;
}

int ZipImageReaderModule::PageCount() const {
  return runtime_.PageCount();
}

int ZipImageReaderModule::CurrentPage() const {
  return runtime_.CurrentPage();
}

ReaderCapabilities ZipImageReaderModule::Capabilities() const {
  ReaderCapabilities capabilities;
  capabilities.supports_rotation = true;
  capabilities.supports_zoom = true;
  capabilities.supports_horizontal_pan = true;
  capabilities.supports_continuous_scroll = true;
  capabilities.is_image_sequence = true;
  return capabilities;
}

bool ZipImageReaderModule::IsRenderPending() const {
  return runtime_.IsRenderPending();
}

const char *ZipImageReaderModule::BackendName() const {
  return runtime_.BackendName();
}

void ZipImageReaderModule::RotateLeft() {
  runtime_.RotateLeft();
}

void ZipImageReaderModule::RotateRight() {
  runtime_.RotateRight();
}

void ZipImageReaderModule::ZoomOut() {
  runtime_.ZoomOut();
}

void ZipImageReaderModule::ZoomIn() {
  runtime_.ZoomIn();
}

void ZipImageReaderModule::ResetView() {
  runtime_.ResetView();
}

bool ZipImageReaderModule::PanHorizontalByPixels(int delta_px) {
  return runtime_.PanHorizontalByPixels(delta_px);
}

void ZipImageReaderModule::ScrollByPixels(int delta_px) {
  runtime_.ScrollByPixels(delta_px);
}

void ZipImageReaderModule::JumpByScreen(int direction) {
  runtime_.JumpByScreen(direction);
}

void ZipImageReaderModule::SetPage(int page_index) {
  runtime_.SetPage(page_index);
}
