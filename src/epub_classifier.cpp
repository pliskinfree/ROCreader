#include "epub_classifier.h"

#include "epub_flow_reader.h"
#include "runtime_log.h"

EpubKind ClassifyEpub(const std::string &path) {
  const bool flow = EpubFlowReader::LooksLikeMixedLayout(path);
  const EpubKind kind = flow ? EpubKind::FlowMixed : EpubKind::ComicImageOnly;
  runtime_log::Line(std::string("[epub_classifier] path=") + path + " kind=" + EpubKindName(kind));
  return kind;
}

const char *EpubKindName(EpubKind kind) {
  switch (kind) {
  case EpubKind::ComicImageOnly: return "comic";
  case EpubKind::FlowMixed: return "flow";
  case EpubKind::Unknown: return "unknown";
  }
  return "unknown";
}
