#include "epub_reader_module.h"

#include "runtime_log.h"

EpubReaderModule::EpubReaderModule()
    : comic_module_(), flow_module_() {}

bool EpubReaderModule::Open(const ReaderOpenRequest &request) {
  restore_progress_ = request.progress;
  active_module_ = nullptr;
  active_kind_ = ClassifyEpub(request.path);

  if (active_kind_ == EpubKind::FlowMixed) {
    if (flow_module_.Open(request)) {
      active_module_ = &flow_module_;
      return true;
    }
    runtime_log::Line("[epub_reader_module] independent flow open failed, falling back to comic path=" +
                      request.path);
  }

  if (comic_module_.Open(request)) {
    active_module_ = &comic_module_;
    active_kind_ = EpubKind::ComicImageOnly;
    return true;
  }

  if (active_kind_ != EpubKind::FlowMixed && flow_module_.Open(request)) {
    active_module_ = &flow_module_;
    active_kind_ = EpubKind::FlowMixed;
    return true;
  }

  active_kind_ = EpubKind::Unknown;
  return false;
}

void EpubReaderModule::Close() {
  if (active_module_) active_module_->Close();
  active_module_ = nullptr;
  active_kind_ = EpubKind::Unknown;
}

bool EpubReaderModule::IsOpen() const {
  return active_module_ ? active_module_->IsOpen() : false;
}

void EpubReaderModule::UpdateViewport(int w, int h) {
  if (active_module_) active_module_->UpdateViewport(w, h);
}

void EpubReaderModule::Tick(float dt) {
  if (active_module_) active_module_->Tick(dt);
}

void EpubReaderModule::Draw(SDL_Renderer *renderer) {
  if (active_module_) active_module_->Draw(renderer);
}

void EpubReaderModule::PrefetchPageAt(int page_index) {
  if (active_module_) active_module_->PrefetchPageAt(page_index);
}

bool EpubReaderModule::DrawPageAt(SDL_Renderer *renderer, int page_index, const SDL_Rect &dst_rect) {
  return active_module_ ? active_module_->DrawPageAt(renderer, page_index, dst_rect) : false;
}

bool EpubReaderModule::CanDrawPageAt(int page_index) const {
  return active_module_ ? active_module_->CanDrawPageAt(page_index) : false;
}

void EpubReaderModule::HandleInput(const InputManager &input, float dt) {
  if (active_module_) active_module_->HandleInput(input, dt);
}

ReaderProgress EpubReaderModule::Progress() const {
  return active_module_ ? active_module_->Progress() : ReaderProgress{};
}

void EpubReaderModule::RestoreProgress(const ReaderProgress &progress) {
  restore_progress_ = progress;
  if (active_module_) active_module_->RestoreProgress(progress);
}

int EpubReaderModule::PageCount() const {
  return active_module_ ? active_module_->PageCount() : 0;
}

int EpubReaderModule::CurrentPage() const {
  return active_module_ ? active_module_->CurrentPage() : 0;
}

ReaderCapabilities EpubReaderModule::Capabilities() const {
  if (active_module_) return active_module_->Capabilities();
  ReaderCapabilities capabilities;
  return capabilities;
}

bool EpubReaderModule::IsRenderPending() const {
  return active_module_ ? active_module_->IsRenderPending() : false;
}

const char *EpubReaderModule::BackendName() const {
  return active_module_ ? active_module_->BackendName() : "none";
}

void EpubReaderModule::RotateLeft() {
  if (active_module_) active_module_->RotateLeft();
}

void EpubReaderModule::RotateRight() {
  if (active_module_) active_module_->RotateRight();
}

void EpubReaderModule::ZoomOut() {
  if (active_module_) active_module_->ZoomOut();
}

void EpubReaderModule::ZoomIn() {
  if (active_module_) active_module_->ZoomIn();
}

void EpubReaderModule::ResetView() {
  if (active_module_) active_module_->ResetView();
}

bool EpubReaderModule::PanHorizontalByPixels(int delta_px) {
  return active_module_ ? active_module_->PanHorizontalByPixels(delta_px) : false;
}

bool EpubReaderModule::PanVerticalByPixels(int delta_px) {
  return active_module_ ? active_module_->PanVerticalByPixels(delta_px) : false;
}

void EpubReaderModule::ScrollByPixels(int delta_px) {
  if (active_module_) active_module_->ScrollByPixels(delta_px);
}

void EpubReaderModule::JumpByScreen(int direction) {
  if (active_module_) active_module_->JumpByScreen(direction);
}

void EpubReaderModule::SetPage(int page_index) {
  if (active_module_) active_module_->SetPage(page_index);
}

std::vector<ReaderChapterAnchor> EpubReaderModule::Chapters() const {
  return active_module_ ? active_module_->Chapters() : std::vector<ReaderChapterAnchor>{};
}

bool EpubReaderModule::ChaptersLoading() const {
  return active_module_ && active_module_->ChaptersLoading();
}

void EpubReaderModule::JumpToChapter(const ReaderChapterAnchor &chapter) {
  if (active_module_) active_module_->JumpToChapter(chapter);
}

void EpubReaderModule::SetFlowBaseFontPointSize(int base_font_pt) {
  flow_module_.SetFlowBaseFontPointSize(base_font_pt);
  if (active_module_ && active_module_ != &flow_module_) {
    active_module_->SetFlowBaseFontPointSize(base_font_pt);
  }
}

void EpubReaderModule::SetFlowColors(SDL_Color background_color, SDL_Color font_color) {
  flow_module_.SetFlowColors(background_color, font_color);
  if (active_module_ && active_module_ != &flow_module_) {
    active_module_->SetFlowColors(background_color, font_color);
  }
}
