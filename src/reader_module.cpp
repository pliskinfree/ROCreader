#include "reader_module.h"

bool NullReaderModule::Open(const ReaderOpenRequest &request) {
  (void)request;
  return false;
}

void NullReaderModule::Close() {}

bool NullReaderModule::IsOpen() const {
  return false;
}

void NullReaderModule::UpdateViewport(int w, int h) {
  (void)w;
  (void)h;
}

void NullReaderModule::Tick(float dt) {
  (void)dt;
}

void NullReaderModule::Draw(SDL_Renderer *renderer) {
  (void)renderer;
}

void NullReaderModule::HandleInput(const InputManager &input, float dt) {
  (void)input;
  (void)dt;
}

ReaderProgress NullReaderModule::Progress() const {
  return ReaderProgress{};
}

void NullReaderModule::RestoreProgress(const ReaderProgress &progress) {
  (void)progress;
}

int NullReaderModule::PageCount() const {
  return 0;
}

int NullReaderModule::CurrentPage() const {
  return 0;
}

ReaderCapabilities NullReaderModule::Capabilities() const {
  return ReaderCapabilities{};
}

