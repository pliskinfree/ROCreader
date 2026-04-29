#pragma once

#include "reader_module.h"

#include <array>

class ReaderManager {
public:
  void Register(ReaderMode mode, IReaderModule *module);
  bool Open(ReaderMode mode, const ReaderOpenRequest &request);
  IReaderModule *Module(ReaderMode mode);
  const IReaderModule *Module(ReaderMode mode) const;
  IReaderModule *Active();
  const IReaderModule *Active() const;
  ReaderMode ActiveMode() const;
  void SetActiveMode(ReaderMode mode);
  void Close(ReaderMode mode);
  void CloseActive();

private:
  static size_t IndexForMode(ReaderMode mode);

  std::array<IReaderModule *, 5> modules_{};
  NullReaderModule null_module_;
  ReaderMode active_mode_ = ReaderMode::None;
};
