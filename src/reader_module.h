#pragma once

#include "input_manager.h"
#include "reader_core.h"

#include <SDL.h>

#include <functional>
#include <string>

struct ReaderCapabilities {
  bool supports_rotation = false;
  bool supports_zoom = false;
  bool supports_horizontal_pan = false;
  bool supports_continuous_scroll = false;
  bool supports_progress_jump = true;
  bool uses_txt_theme = false;
  bool is_image_sequence = false;
  bool is_flow_layout = false;
};

struct ReaderOpenRequest {
  SDL_Renderer *renderer = nullptr;
  std::string path;
  int screen_w = 0;
  int screen_h = 0;
  ReaderProgress progress;
  int flow_base_font_pt = 18;
  SDL_Color flow_background_color{250, 249, 244, 255};
  SDL_Color flow_font_color{28, 28, 28, 255};
};

class IReaderModule {
public:
  virtual ~IReaderModule() = default;

  virtual bool Open(const ReaderOpenRequest &request) = 0;
  virtual void Close() = 0;
  virtual bool IsOpen() const = 0;

  virtual void UpdateViewport(int w, int h) = 0;
  virtual void Tick(float dt) = 0;
  virtual void Draw(SDL_Renderer *renderer) = 0;
  virtual void HandleInput(const InputManager &input, float dt) = 0;

  virtual ReaderProgress Progress() const = 0;
  virtual void RestoreProgress(const ReaderProgress &progress) = 0;
  virtual int PageCount() const = 0;
  virtual int CurrentPage() const = 0;

  virtual ReaderCapabilities Capabilities() const = 0;
  virtual bool IsRenderPending() const { return false; }
  virtual const char *BackendName() const { return "reader-module"; }
  virtual void RotateLeft() {}
  virtual void RotateRight() {}
  virtual void ZoomOut() {}
  virtual void ZoomIn() {}
  virtual void ResetView() {}
  virtual bool PanHorizontalByPixels(int delta_px) {
    (void)delta_px;
    return false;
  }
  virtual void ScrollByPixels(int delta_px) { (void)delta_px; }
  virtual void JumpByScreen(int direction) { (void)direction; }
  virtual void SetPage(int page_index) { (void)page_index; }
  virtual void SetFlowBaseFontPointSize(int base_font_pt) { (void)base_font_pt; }
  virtual void SetFlowColors(SDL_Color background_color, SDL_Color font_color) {
    (void)background_color;
    (void)font_color;
  }
};

class NullReaderModule final : public IReaderModule {
public:
  bool Open(const ReaderOpenRequest &request) override;
  void Close() override;
  bool IsOpen() const override;

  void UpdateViewport(int w, int h) override;
  void Tick(float dt) override;
  void Draw(SDL_Renderer *renderer) override;
  void HandleInput(const InputManager &input, float dt) override;

  ReaderProgress Progress() const override;
  void RestoreProgress(const ReaderProgress &progress) override;
  int PageCount() const override;
  int CurrentPage() const override;

  ReaderCapabilities Capabilities() const override;
};
