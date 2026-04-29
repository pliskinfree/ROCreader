#include "reader_manager.h"

namespace {
constexpr size_t kReaderModuleCount = 5;
}

size_t ReaderManager::IndexForMode(ReaderMode mode) {
  const size_t index = static_cast<size_t>(mode);
  return index < kReaderModuleCount ? index : 0;
}

void ReaderManager::Register(ReaderMode mode, IReaderModule *module) {
  modules_[IndexForMode(mode)] = module;
}

bool ReaderManager::Open(ReaderMode mode, const ReaderOpenRequest &request) {
  IReaderModule *module = Module(mode);
  if (!module->Open(request)) return false;
  active_mode_ = mode;
  return true;
}

IReaderModule *ReaderManager::Module(ReaderMode mode) {
  IReaderModule *module = modules_[IndexForMode(mode)];
  return module ? module : &null_module_;
}

const IReaderModule *ReaderManager::Module(ReaderMode mode) const {
  const IReaderModule *module = modules_[IndexForMode(mode)];
  return module ? module : &null_module_;
}

IReaderModule *ReaderManager::Active() {
  return Module(active_mode_);
}

const IReaderModule *ReaderManager::Active() const {
  return Module(active_mode_);
}

ReaderMode ReaderManager::ActiveMode() const {
  return active_mode_;
}

void ReaderManager::SetActiveMode(ReaderMode mode) {
  active_mode_ = mode;
}

void ReaderManager::Close(ReaderMode mode) {
  Module(mode)->Close();
  if (active_mode_ == mode) active_mode_ = ReaderMode::None;
}

void ReaderManager::CloseActive() {
  Active()->Close();
  active_mode_ = ReaderMode::None;
}
