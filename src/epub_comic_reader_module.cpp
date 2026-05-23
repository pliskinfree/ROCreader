#include "epub_comic_reader_module.h"

EpubComicReaderModule::EpubComicReaderModule() = default;

bool EpubComicReaderModule::Open(const ReaderOpenRequest &request) {
  restore_progress_ = request.progress;
  return runtime_.Open(request.renderer,
                       request.path,
                       request.screen_w,
                       request.screen_h,
                       ToEpubRuntimeProgress(request.progress));
}

void EpubComicReaderModule::Close() {
  runtime_.Close();
}

bool EpubComicReaderModule::IsOpen() const {
  return runtime_.IsOpen();
}

void EpubComicReaderModule::UpdateViewport(int w, int h) {
  runtime_.UpdateViewport(w, h);
}

void EpubComicReaderModule::Tick(float dt) {
  (void)dt;
  runtime_.Tick();
}

void EpubComicReaderModule::Draw(SDL_Renderer *renderer) {
  runtime_.Draw(renderer);
}

void EpubComicReaderModule::PrefetchPageAt(int page_index) {
  runtime_.PrefetchPageAt(page_index);
}

bool EpubComicReaderModule::DrawPageAt(SDL_Renderer *renderer, int page_index, const SDL_Rect &dst_rect) {
  return runtime_.DrawPageAt(renderer, page_index, dst_rect);
}

bool EpubComicReaderModule::CanDrawPageAt(int page_index) const {
  return runtime_.CanDrawPageAt(page_index);
}

void EpubComicReaderModule::HandleInput(const InputManager &input, float dt) {
  (void)input;
  (void)dt;
}

ReaderProgress EpubComicReaderModule::Progress() const {
  return ToReaderProgress(runtime_.Progress());
}

void EpubComicReaderModule::RestoreProgress(const ReaderProgress &progress) {
  restore_progress_ = progress;
}

int EpubComicReaderModule::PageCount() const {
  return runtime_.PageCount();
}

int EpubComicReaderModule::CurrentPage() const {
  return runtime_.CurrentPage();
}

ReaderCapabilities EpubComicReaderModule::Capabilities() const {
  ReaderCapabilities capabilities;
  capabilities.supports_rotation = true;
  capabilities.supports_zoom = true;
  capabilities.supports_horizontal_pan = true;
  capabilities.supports_continuous_scroll = true;
  capabilities.is_image_sequence = true;
  return capabilities;
}

bool EpubComicReaderModule::IsRenderPending() const {
  return runtime_.IsRenderPending();
}

const char *EpubComicReaderModule::BackendName() const {
  return runtime_.BackendName();
}

void EpubComicReaderModule::RotateLeft() {
  runtime_.RotateLeft();
}

void EpubComicReaderModule::RotateRight() {
  runtime_.RotateRight();
}

void EpubComicReaderModule::ZoomOut() {
  runtime_.ZoomOut();
}

void EpubComicReaderModule::ZoomIn() {
  runtime_.ZoomIn();
}

void EpubComicReaderModule::ResetView() {
  runtime_.ResetView();
}

bool EpubComicReaderModule::PanHorizontalByPixels(int delta_px) {
  return runtime_.PanHorizontalByPixels(delta_px);
}

bool EpubComicReaderModule::PanVerticalByPixels(int delta_px) {
  return runtime_.PanVerticalByPixels(delta_px);
}

void EpubComicReaderModule::ScrollByPixels(int delta_px) {
  runtime_.ScrollByPixels(delta_px);
}

void EpubComicReaderModule::JumpByScreen(int direction) {
  runtime_.JumpByScreen(direction);
}

void EpubComicReaderModule::SetPage(int page_index) {
  runtime_.SetPage(page_index);
}
