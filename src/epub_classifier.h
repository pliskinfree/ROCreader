#pragma once

#include <string>

enum class EpubKind {
  Unknown,
  ComicImageOnly,
  FlowMixed,
};

EpubKind ClassifyEpub(const std::string &path);
const char *EpubKindName(EpubKind kind);
