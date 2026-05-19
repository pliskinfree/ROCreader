#pragma once

#include <SDL.h>

#include <string>

#include "epub_runtime.h"

class EpubComicRuntime {
public:
  EpubComicRuntime();
  ~EpubComicRuntime();

  bool Open(SDL_Renderer *renderer, const std::string &path, int screen_w, int screen_h,
            const EpubRuntimeProgress &initial_progress);
  void Close();

  bool IsOpen() const;
  bool HasRealRenderer() const;
  bool IsRenderPending() const;
  const char *BackendName() const;
  void UpdateViewport(int screen_w, int screen_h);
  void Tick();
  void Draw(SDL_Renderer *renderer) const;
  bool DrawPageAt(SDL_Renderer *renderer, int page_index, const SDL_Rect &dst_rect) const;

  void RotateLeft();
  void RotateRight();
  void ZoomOut();
  void ZoomIn();
  void ResetView();
  bool PanHorizontalByPixels(int delta_px);
  bool PanVerticalByPixels(int delta_px);
  void ScrollByPixels(int delta_px);
  void JumpByScreen(int direction);
  void SetPage(int page_index);

  int PageCount() const;
  bool PageSize(int page_index, int &w, int &h) const;
  int CurrentPage() const;
  EpubRuntimeProgress Progress() const;

private:
  struct Impl;
  Impl *impl_ = nullptr;
};

