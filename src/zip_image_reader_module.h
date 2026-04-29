#pragma once

#include "reader_module.h"
#include "zip_image_runtime.h"

class ZipImageReaderModule final : public IReaderModule {
public:
  explicit ZipImageReaderModule(ZipImageRuntime &runtime);

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
  ZipImageRuntime &runtime_;
  ReaderProgress restore_progress_;
};

