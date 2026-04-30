#pragma once

#include "reader_session_state.h"

#include <string>
#include <vector>

std::vector<ReaderChapterAnchor> BuildTxtChapterAnchors(const TxtReaderState &state);
void ResetTxtChapterScan(ReaderUiState &ui, const TxtReaderState &state);
bool TickTxtChapterScan(ReaderUiState &ui, const TxtReaderState &state, size_t line_budget);
ReaderChapterAnchor MakeBodyChapterAnchor();
