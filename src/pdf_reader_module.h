#pragma once

#include "pdf_runtime.h"
#include "reader_module.h"

class PdfReaderModule final : public IReaderModule {
public:
  explicit PdfReaderModule(PdfRuntime &runtime);

  bool Open(const ReaderOpenRequest &request) override;
  void Close() override;
  bool IsOpen() const override;
  void UpdateViewport(int w, int h) override;
  void Tick(float dt) override;
  void Draw(SDL_Renderer *renderer) override;
  bool DrawPageAt(SDL_Renderer *renderer, int page_index, const SDL_Rect &dst_rect) override;
  void HandleInput(const InputManager &input, float dt) override;
  ReaderProgress Progress() const override;
  void RestoreProgress(const ReaderProgress &progress) override;
  int PageCount() const override;
  int CurrentPage() const override;
  ReaderCapabilities Capabilities() const override;
  bool IsRenderPending() const override;
  const char *BackendName() const override;
  void RotateLeft() override;
  void RotateRight() override;
  void ZoomOut() override;
  void ZoomIn() override;
  void ResetView() override;
  bool PanHorizontalByPixels(int delta_px) override;
  bool PanVerticalByPixels(int delta_px) override;
  void ScrollByPixels(int delta_px) override;
  void JumpByScreen(int direction) override;
  void SetPage(int page_index) override;

private:
  PdfRuntime &runtime_;
  ReaderProgress restore_progress_;
};
