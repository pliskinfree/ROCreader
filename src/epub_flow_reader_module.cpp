#include "epub_flow_reader_module.h"

EpubFlowReaderModule::EpubFlowReaderModule() = default;

bool EpubFlowReaderModule::Open(const ReaderOpenRequest &request) {
  restore_progress_ = request.progress;
  return reader_.Open(request.path,
                      request.renderer,
                      request.screen_w,
                      request.screen_h,
                      ToEpubRuntimeProgress(request.progress),
                      request.flow_base_font_pt,
                      request.flow_background_color,
                      request.flow_font_color);
}

void EpubFlowReaderModule::Close() {
  reader_.Close();
}

bool EpubFlowReaderModule::IsOpen() const {
  return reader_.IsOpen();
}

void EpubFlowReaderModule::UpdateViewport(int w, int h) {
  reader_.UpdateViewport(w, h);
}

void EpubFlowReaderModule::Tick(float dt) {
  (void)dt;
  reader_.Tick();
}

void EpubFlowReaderModule::Draw(SDL_Renderer *renderer) {
  reader_.Draw(renderer);
}

void EpubFlowReaderModule::HandleInput(const InputManager &input, float dt) {
  (void)input;
  (void)dt;
}

ReaderProgress EpubFlowReaderModule::Progress() const {
  return ToReaderProgress(reader_.Progress());
}

void EpubFlowReaderModule::RestoreProgress(const ReaderProgress &progress) {
  restore_progress_ = progress;
}

int EpubFlowReaderModule::PageCount() const {
  return reader_.PageCount();
}

int EpubFlowReaderModule::CurrentPage() const {
  return reader_.CurrentPage();
}

ReaderCapabilities EpubFlowReaderModule::Capabilities() const {
  ReaderCapabilities capabilities;
  capabilities.supports_continuous_scroll = true;
  capabilities.uses_txt_theme = true;
  capabilities.is_flow_layout = true;
  return capabilities;
}

bool EpubFlowReaderModule::IsRenderPending() const {
  return reader_.IsRenderPending();
}

const char *EpubFlowReaderModule::BackendName() const {
  return reader_.BackendName();
}

void EpubFlowReaderModule::ResetView() {
  reader_.ResetView();
}

void EpubFlowReaderModule::ScrollByPixels(int delta_px) {
  reader_.ScrollByPixels(delta_px);
}

void EpubFlowReaderModule::JumpByScreen(int direction) {
  reader_.JumpByScreen(direction);
}

void EpubFlowReaderModule::SetPage(int page_index) {
  reader_.SetPage(page_index);
}

std::vector<ReaderChapterAnchor> EpubFlowReaderModule::Chapters() const {
  return reader_.Chapters();
}

bool EpubFlowReaderModule::ChaptersLoading() const {
  return reader_.ChaptersLoading();
}

int EpubFlowReaderModule::ChaptersLoadingPercent() const {
  return reader_.ChaptersLoadingPercent();
}

void EpubFlowReaderModule::JumpToChapter(const ReaderChapterAnchor &chapter) {
  reader_.JumpToChapter(chapter);
}

void EpubFlowReaderModule::SetFlowBaseFontPointSize(int base_font_pt) {
  reader_.SetBaseFontPointSize(base_font_pt);
}

void EpubFlowReaderModule::SetFlowColors(SDL_Color background_color, SDL_Color font_color) {
  reader_.SetColors(background_color, font_color);
}
