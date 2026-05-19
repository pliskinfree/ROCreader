#pragma once

#include <SDL.h>

#include <string>

enum class EpubRuntimeOpenMode;

struct EpubRuntimeProgress {
  int page = 0;
  int rotation = 0;
  float zoom = 1.0f;
  int scroll_x = 0;
  int scroll_y = 0;
};

class EpubRuntime {
public:
  EpubRuntime();
  ~EpubRuntime();

  bool Open(SDL_Renderer *renderer, const std::string &path, int screen_w, int screen_h,
            const EpubRuntimeProgress &initial_progress, int flow_base_font_pt = 18,
            SDL_Color flow_background_color = SDL_Color{250, 249, 244, 255},
            SDL_Color flow_font_color = SDL_Color{28, 28, 28, 255});
  bool OpenWithMode(SDL_Renderer *renderer, const std::string &path, int screen_w, int screen_h,
                    const EpubRuntimeProgress &initial_progress, EpubRuntimeOpenMode mode,
                    int flow_base_font_pt = 18,
                    SDL_Color flow_background_color = SDL_Color{250, 249, 244, 255},
                    SDL_Color flow_font_color = SDL_Color{28, 28, 28, 255});
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
  void SetFlowBaseFontPointSize(int base_font_pt);
  void SetFlowColors(SDL_Color background_color, SDL_Color font_color);
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

