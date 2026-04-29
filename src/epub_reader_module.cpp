#include "epub_reader_module.h"

#include <string>

EpubReaderModule::EpubReaderModule(EpubRuntime &runtime) : runtime_(runtime) {}

bool EpubReaderModule::Open(const ReaderOpenRequest &request) {
  EpubRuntimeProgress progress;
  progress.page = request.progress.page;
  progress.rotation = request.progress.rotation;
  progress.zoom = request.progress.zoom;
  progress.scroll_x = request.progress.scroll_x;
  progress.scroll_y = request.progress.scroll_y;
  restore_progress_ = request.progress;
  return runtime_.Open(request.renderer,
                       request.path,
                       request.screen_w,
                       request.screen_h,
                       progress,
                       request.flow_base_font_pt,
                       request.flow_background_color,
                       request.flow_font_color);
}

void EpubReaderModule::Close() {
  runtime_.Close();
}

bool EpubReaderModule::IsOpen() const {
  return runtime_.IsOpen();
}

void EpubReaderModule::UpdateViewport(int w, int h) {
  runtime_.UpdateViewport(w, h);
}

void EpubReaderModule::Tick(float dt) {
  (void)dt;
  runtime_.Tick();
}

void EpubReaderModule::Draw(SDL_Renderer *renderer) {
  runtime_.Draw(renderer);
}

void EpubReaderModule::HandleInput(const InputManager &input, float dt) {
  (void)input;
  (void)dt;
}

ReaderProgress EpubReaderModule::Progress() const {
  const EpubRuntimeProgress progress = runtime_.Progress();
  return ReaderProgress{progress.page, progress.rotation, progress.zoom, progress.scroll_x, progress.scroll_y};
}

void EpubReaderModule::RestoreProgress(const ReaderProgress &progress) {
  restore_progress_ = progress;
}

int EpubReaderModule::PageCount() const {
  return runtime_.PageCount();
}

int EpubReaderModule::CurrentPage() const {
  return runtime_.CurrentPage();
}

ReaderCapabilities EpubReaderModule::Capabilities() const {
  ReaderCapabilities capabilities;
  const bool flow_epub = std::string(runtime_.BackendName()) == "epub-flow";
  capabilities.supports_rotation = !flow_epub;
  capabilities.supports_zoom = !flow_epub;
  capabilities.supports_horizontal_pan = !flow_epub;
  capabilities.supports_continuous_scroll = true;
  capabilities.uses_txt_theme = flow_epub;
  capabilities.is_image_sequence = !flow_epub;
  capabilities.is_flow_layout = flow_epub;
  return capabilities;
}

