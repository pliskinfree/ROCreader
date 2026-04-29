#pragma once

#include "epub_runtime.h"
#include "reader_module.h"

enum class EpubRuntimeOpenMode {
  Auto,
  Comic,
  Flow,
};

ReaderProgress ToReaderProgress(const EpubRuntimeProgress &progress);
EpubRuntimeProgress ToEpubRuntimeProgress(const ReaderProgress &progress);
bool IsEpubFlowBackend(const EpubRuntime &runtime);
