#pragma once

#include "epub_common.h"
#include "epub_flow_reader.h"

class EpubFlowReaderModule final : public IReaderModule {
public:
  EpubFlowReaderModule();

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
  void ResetView() override;
  void ScrollByPixels(int delta_px) override;
  void JumpByScreen(int direction) override;
  void SetPage(int page_index) override;
  std::vector<ReaderChapterAnchor> Chapters() const override;
  bool ChaptersLoading() const override;
  int ChaptersLoadingPercent() const override;
  void JumpToChapter(const ReaderChapterAnchor &chapter) override;
  void SetFlowBaseFontPointSize(int base_font_pt) override;
  void SetFlowColors(SDL_Color background_color, SDL_Color font_color) override;

private:
  EpubFlowReader reader_;
  ReaderProgress restore_progress_;
};
