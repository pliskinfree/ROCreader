#include "epub_runtime.h"

#include "epub_comic_runtime.h"
#include "epub_common.h"
#include "epub_flow_reader.h"
#include "runtime_log.h"

#include <algorithm>
#include <string>

struct EpubRuntime::Impl {
  EpubComicRuntime comic_runtime;
  EpubFlowReader flow_reader;
  bool flow_mode = false;
  int flow_base_font_pt = 18;
  SDL_Color flow_background_color{250, 249, 244, 255};
  SDL_Color flow_font_color{28, 28, 28, 255};
  std::string path;
};

EpubRuntime::EpubRuntime() : impl_(new Impl()) {}

EpubRuntime::~EpubRuntime() {
  Close();
  delete impl_;
  impl_ = nullptr;
}

bool EpubRuntime::Open(SDL_Renderer *renderer,
                       const std::string &path,
                       int screen_w,
                       int screen_h,
                       const EpubRuntimeProgress &initial_progress,
                       int flow_base_font_pt,
                       SDL_Color flow_background_color,
                       SDL_Color flow_font_color) {
  return OpenWithMode(renderer,
                      path,
                      screen_w,
                      screen_h,
                      initial_progress,
                      EpubRuntimeOpenMode::Auto,
                      flow_base_font_pt,
                      flow_background_color,
                      flow_font_color);
}

bool EpubRuntime::OpenWithMode(SDL_Renderer *renderer,
                               const std::string &path,
                               int screen_w,
                               int screen_h,
                               const EpubRuntimeProgress &initial_progress,
                               EpubRuntimeOpenMode mode,
                               int flow_base_font_pt,
                               SDL_Color flow_background_color,
                               SDL_Color flow_font_color) {
  Close();
  if (!impl_) return false;

  impl_->flow_base_font_pt = flow_base_font_pt;
  impl_->flow_background_color = flow_background_color;
  impl_->flow_font_color = flow_font_color;

  const int view_w = std::max(1, screen_w);
  const int view_h = std::max(1, screen_h);
  const bool try_flow = mode == EpubRuntimeOpenMode::Flow ||
                        (mode == EpubRuntimeOpenMode::Auto && EpubFlowReader::LooksLikeMixedLayout(path));
  const bool allow_comic_fallback = mode != EpubRuntimeOpenMode::Flow;

  if (try_flow &&
      impl_->flow_reader.Open(path, renderer, view_w, view_h, initial_progress, impl_->flow_base_font_pt,
                              impl_->flow_background_color, impl_->flow_font_color)) {
    impl_->flow_mode = true;
    impl_->path = path;
    runtime_log::Line("[epub_runtime] using flow proxy path=" + path);
    return true;
  }

  if (!allow_comic_fallback) {
    runtime_log::Line("[epub_runtime] forced flow open failed path=" + path);
    return false;
  }

  if (!impl_->comic_runtime.Open(renderer, path, view_w, view_h, initial_progress)) {
    runtime_log::Line("[epub_runtime] comic proxy open failed path=" + path);
    return false;
  }
  impl_->flow_mode = false;
  impl_->path = path;
  runtime_log::Line("[epub_runtime] using comic proxy path=" + path);
  return true;
}

void EpubRuntime::Close() {
  if (!impl_) return;
  impl_->flow_reader.Close();
  impl_->comic_runtime.Close();
  impl_->flow_mode = false;
  impl_->path.clear();
}

bool EpubRuntime::IsOpen() const {
  if (!impl_) return false;
  return impl_->flow_mode ? impl_->flow_reader.IsOpen() : impl_->comic_runtime.IsOpen();
}

bool EpubRuntime::HasRealRenderer() const {
  if (!impl_) return false;
  return impl_->flow_mode ? impl_->flow_reader.HasRealRenderer() : impl_->comic_runtime.HasRealRenderer();
}

const char *EpubRuntime::BackendName() const {
  if (!impl_) return "none";
  return impl_->flow_mode ? impl_->flow_reader.BackendName() : impl_->comic_runtime.BackendName();
}

bool EpubRuntime::IsRenderPending() const {
  if (!impl_) return false;
  return impl_->flow_mode ? impl_->flow_reader.IsRenderPending() : impl_->comic_runtime.IsRenderPending();
}

void EpubRuntime::UpdateViewport(int screen_w, int screen_h) {
  if (!impl_) return;
  if (impl_->flow_mode) impl_->flow_reader.UpdateViewport(screen_w, screen_h);
  else impl_->comic_runtime.UpdateViewport(screen_w, screen_h);
}

void EpubRuntime::Tick() {
  if (!impl_) return;
  if (impl_->flow_mode) impl_->flow_reader.Tick();
  else impl_->comic_runtime.Tick();
}

void EpubRuntime::Draw(SDL_Renderer *renderer) const {
  if (!impl_) return;
  if (impl_->flow_mode) impl_->flow_reader.Draw(renderer);
  else impl_->comic_runtime.Draw(renderer);
}

bool EpubRuntime::DrawPageAt(SDL_Renderer *renderer, int page_index, const SDL_Rect &dst_rect) const {
  if (!impl_ || impl_->flow_mode) return false;
  return impl_->comic_runtime.DrawPageAt(renderer, page_index, dst_rect);
}

void EpubRuntime::RotateLeft() {
  if (!impl_) return;
  if (impl_->flow_mode) impl_->flow_reader.RotateLeft();
  else impl_->comic_runtime.RotateLeft();
}

void EpubRuntime::RotateRight() {
  if (!impl_) return;
  if (impl_->flow_mode) impl_->flow_reader.RotateRight();
  else impl_->comic_runtime.RotateRight();
}

void EpubRuntime::ZoomOut() {
  if (!impl_) return;
  if (impl_->flow_mode) impl_->flow_reader.ZoomOut();
  else impl_->comic_runtime.ZoomOut();
}

void EpubRuntime::ZoomIn() {
  if (!impl_) return;
  if (impl_->flow_mode) impl_->flow_reader.ZoomIn();
  else impl_->comic_runtime.ZoomIn();
}

void EpubRuntime::ResetView() {
  if (!impl_) return;
  if (impl_->flow_mode) impl_->flow_reader.ResetView();
  else impl_->comic_runtime.ResetView();
}

void EpubRuntime::SetFlowBaseFontPointSize(int base_font_pt) {
  if (!impl_) return;
  impl_->flow_base_font_pt = std::max(8, base_font_pt);
  if (impl_->flow_mode) impl_->flow_reader.SetBaseFontPointSize(impl_->flow_base_font_pt);
}

void EpubRuntime::SetFlowColors(SDL_Color background_color, SDL_Color font_color) {
  if (!impl_) return;
  impl_->flow_background_color = background_color;
  impl_->flow_font_color = font_color;
  if (impl_->flow_mode) impl_->flow_reader.SetColors(background_color, font_color);
}

bool EpubRuntime::PanHorizontalByPixels(int delta_px) {
  if (!impl_ || impl_->flow_mode) return false;
  return impl_->comic_runtime.PanHorizontalByPixels(delta_px);
}

bool EpubRuntime::PanVerticalByPixels(int delta_px) {
  if (!impl_ || impl_->flow_mode) return false;
  return impl_->comic_runtime.PanVerticalByPixels(delta_px);
}

void EpubRuntime::ScrollByPixels(int delta_px) {
  if (!impl_) return;
  if (impl_->flow_mode) impl_->flow_reader.ScrollByPixels(delta_px);
  else impl_->comic_runtime.ScrollByPixels(delta_px);
}

void EpubRuntime::JumpByScreen(int direction) {
  if (!impl_) return;
  if (impl_->flow_mode) impl_->flow_reader.JumpByScreen(direction);
  else impl_->comic_runtime.JumpByScreen(direction);
}

void EpubRuntime::SetPage(int page_index) {
  if (!impl_) return;
  if (impl_->flow_mode) impl_->flow_reader.SetPage(page_index);
  else impl_->comic_runtime.SetPage(page_index);
}

int EpubRuntime::PageCount() const {
  if (!impl_) return 0;
  return impl_->flow_mode ? impl_->flow_reader.PageCount() : impl_->comic_runtime.PageCount();
}

bool EpubRuntime::PageSize(int page_index, int &w, int &h) const {
  if (!impl_) return false;
  return impl_->flow_mode ? impl_->flow_reader.PageSize(page_index, w, h)
                          : impl_->comic_runtime.PageSize(page_index, w, h);
}

int EpubRuntime::CurrentPage() const {
  if (!impl_) return 0;
  return impl_->flow_mode ? impl_->flow_reader.CurrentPage() : impl_->comic_runtime.CurrentPage();
}

EpubRuntimeProgress EpubRuntime::Progress() const {
  if (!impl_) return {};
  return impl_->flow_mode ? impl_->flow_reader.Progress() : impl_->comic_runtime.Progress();
}
