#include "epub_common.h"

#include <string>

ReaderProgress ToReaderProgress(const EpubRuntimeProgress &progress) {
  return ReaderProgress{progress.page, progress.rotation, progress.zoom, progress.scroll_x, progress.scroll_y};
}

EpubRuntimeProgress ToEpubRuntimeProgress(const ReaderProgress &progress) {
  EpubRuntimeProgress out;
  out.page = progress.page;
  out.rotation = progress.rotation;
  out.zoom = progress.zoom;
  out.scroll_x = progress.scroll_x;
  out.scroll_y = progress.scroll_y;
  return out;
}

bool IsEpubFlowBackend(const EpubRuntime &runtime) {
  return std::string(runtime.BackendName()) == "epub-flow";
}
