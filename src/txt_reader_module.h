#pragma once

#include "reader_module.h"
#include "reader_session_state.h"

#include <functional>

struct TxtReaderModuleCallbacks {
  std::function<bool(const std::string &)> open_text_book;
  std::function<void()> close_text_reader;
  std::function<void(int)> text_scroll_by;
  std::function<void(int)> text_page_by;
  std::function<void(int)> text_jump_to_percent;
};

class TxtReaderModule final : public IReaderModule {
public:
  TxtReaderModule(ReaderUiState &ui, TxtReaderModuleCallbacks callbacks);

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

private:
  ReaderUiState &ui_;
  TxtReaderModuleCallbacks callbacks_;
};

