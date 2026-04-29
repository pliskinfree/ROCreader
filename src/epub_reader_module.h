#pragma once

#include "epub_classifier.h"
#include "epub_comic_reader_module.h"
#include "epub_flow_reader_module.h"
#include "reader_module.h"

class EpubReaderModule final : public IReaderModule {
public:
  EpubReaderModule();

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
  bool IsRenderPending() const override;
  const char *BackendName() const override;
  void RotateLeft() override;
  void RotateRight() override;
  void ZoomOut() override;
  void ZoomIn() override;
  void ResetView() override;
  bool PanHorizontalByPixels(int delta_px) override;
  void ScrollByPixels(int delta_px) override;
  void JumpByScreen(int direction) override;
  void SetPage(int page_index) override;
  void SetFlowBaseFontPointSize(int base_font_pt) override;
  void SetFlowColors(SDL_Color background_color, SDL_Color font_color) override;

private:
  EpubComicReaderModule comic_module_;
  EpubFlowReaderModule flow_module_;
  IReaderModule *active_module_ = nullptr;
  EpubKind active_kind_ = EpubKind::Unknown;
  ReaderProgress restore_progress_;
};
