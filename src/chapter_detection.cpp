#include "chapter_detection.h"

#include <algorithm>
#include <cctype>
#include <climits>
#include <regex>
#include <unordered_set>

namespace {
constexpr size_t kMaxChapterTitleBytes = 96;
constexpr size_t kMinRawLinesForChapterList = 80;
constexpr size_t kMaxChapterAnchors = 5000;

std::string Trim(const std::string &s) {
  size_t first = 0;
  while (first < s.size() && std::isspace(static_cast<unsigned char>(s[first]))) ++first;
  size_t last = s.size();
  while (last > first && std::isspace(static_cast<unsigned char>(s[last - 1]))) --last;
  return s.substr(first, last - first);
}

std::string CollapseSpaces(const std::string &s) {
  std::string out;
  bool pending_space = false;
  for (unsigned char c : s) {
    if (std::isspace(c)) {
      pending_space = true;
      continue;
    }
    if (pending_space && !out.empty()) out.push_back(' ');
    pending_space = false;
    out.push_back(static_cast<char>(c));
  }
  return out;
}

bool StartsWith(const std::string &s, const char *prefix) {
  const std::string p(prefix);
  return s.rfind(p, 0) == 0;
}

bool IsAsciiDigit(char c) {
  return c >= '0' && c <= '9';
}

size_t ConsumeAsciiNumber(const std::string &s, size_t pos) {
  const size_t start = pos;
  while (pos < s.size() && IsAsciiDigit(s[pos])) ++pos;
  return pos > start ? pos : start;
}

size_t ConsumeCnNumber(const std::string &s, size_t pos) {
  static const char *digits[] = {
      u8"\u96f6", u8"\u3007", u8"\u4e00", u8"\u4e8c", u8"\u4e09", u8"\u56db",
      u8"\u4e94", u8"\u516d", u8"\u4e03", u8"\u516b", u8"\u4e5d", u8"\u5341",
      u8"\u767e", u8"\u5343", u8"\u4e07", u8"\u4ebf", u8"\u4e24", u8"\u5169",
  };
  bool consumed = false;
  bool matched = true;
  while (pos < s.size() && matched) {
    matched = false;
    for (const char *digit : digits) {
      const std::string d(digit);
      if (s.compare(pos, d.size(), d) == 0) {
        pos += d.size();
        consumed = true;
        matched = true;
        break;
      }
    }
  }
  return consumed ? pos : std::string::npos;
}

bool ConsumeOneOf(const std::string &s, size_t &pos, const std::vector<const char *> &tokens) {
  for (const char *token : tokens) {
    const std::string t(token);
    if (s.compare(pos, t.size(), t) == 0) {
      pos += t.size();
      return true;
    }
  }
  return false;
}

bool TailAllowedAfterMarker(const std::string &tail) {
  if (tail.empty()) return true;
  const unsigned char first = static_cast<unsigned char>(tail.front());
  const std::string t = Trim(tail);
  if (t.empty()) return true;
  if (t.size() > 42) return false;
  if (std::isspace(first) || StartsWith(tail, u8"\u3000") ||
      StartsWith(t, u8"\uff1a") || StartsWith(t, ":") ||
      StartsWith(t, "-") || StartsWith(t, u8"\u2014") ||
      StartsWith(t, u8"\uff0d")) {
    return true;
  }
  return false;
}

bool LooksLikeZhChapterTitle(const std::string &text) {
  if (text.empty() || text.size() > kMaxChapterTitleBytes) return false;
  size_t pos = 0;
  if (StartsWith(text, u8"\u7b2c")) {
    pos = std::string(u8"\u7b2c").size();
    size_t next = ConsumeAsciiNumber(text, pos);
    if (next == pos) next = ConsumeCnNumber(text, pos);
    if (next == std::string::npos || next == pos) return false;
    pos = next;
    static const std::vector<const char *> markers = {
        u8"\u7ae0", u8"\u7bc0", u8"\u8282", u8"\u5377", u8"\u56de",
        u8"\u90e8", u8"\u7bc7", u8"\u96c6", u8"\u8bdd", u8"\u8a71",
        u8"\u5e55",
    };
    if (!ConsumeOneOf(text, pos, markers)) return false;
    return TailAllowedAfterMarker(text.substr(pos));
  }

  static const std::vector<const char *> standalone = {
      u8"\u5e8f\u7ae0", u8"\u5e8f\u8282", u8"\u6954\u5b50", u8"\u7ec8\u7ae0",
      u8"\u7d42\u7ae0", u8"\u5c3e\u58f0", u8"\u5c3e\u8072", u8"\u540e\u8bb0",
      u8"\u5f8c\u8a18",
  };
  size_t marker_pos = 0;
  if (ConsumeOneOf(text, marker_pos, standalone)) {
    return TailAllowedAfterMarker(text.substr(marker_pos));
  }

  if (StartsWith(text, u8"\u5377")) {
    pos = std::string(u8"\u5377").size();
    size_t next = ConsumeAsciiNumber(text, pos);
    if (next == pos) next = ConsumeCnNumber(text, pos);
    if (next == std::string::npos || next == pos) return false;
    return TailAllowedAfterMarker(text.substr(next));
  }
  return false;
}

bool LooksLikeEnChapterTitle(const std::string &text) {
  if (text.empty() || text.size() > kMaxChapterTitleBytes) return false;
  static const std::regex en_re(
      R"(^(chapter|part|book|volume)\s+([0-9]+|[ivxlcdm]+)(\s*[:\-].{0,40})?$)",
      std::regex::icase);
  return std::regex_match(text, en_re);
}

bool LooksLikeChapterTitle(const std::string &line) {
  const std::string text = CollapseSpaces(Trim(line));
  return LooksLikeZhChapterTitle(text) || LooksLikeEnChapterTitle(text);
}

bool HasEnoughRawGap(size_t prev_line, size_t line) {
  return line > prev_line && line - prev_line >= 12;
}

bool HasEnoughWrappedGap(const TxtReaderState &state, size_t prev_line, size_t line) {
  if (line <= prev_line) return false;
  size_t meaningful = 0;
  for (size_t i = prev_line + 1; i < line; ++i) {
    if (Trim(state.lines[i]).size() >= 18) ++meaningful;
    if (meaningful >= 8) return true;
  }
  return line - prev_line >= 24;
}

bool LooksLikeTocNoise(const std::vector<ReaderChapterAnchor> &anchors, size_t total_lines) {
  if (anchors.size() < 6 || total_lines < 120) return false;
  size_t early = 0;
  for (const auto &anchor : anchors) {
    const size_t line = static_cast<size_t>(std::max(0, anchor.page));
    if (line < total_lines / 10 || line < 80) ++early;
  }
  return early >= 6 && early * 2 >= anchors.size();
}

void AddAnchor(std::vector<ReaderChapterAnchor> &anchors, std::unordered_set<std::string> &seen,
               std::string title, size_t line, size_t source_offset, int line_h) {
  title = CollapseSpaces(Trim(title));
  if (title.empty() || !seen.insert(title).second) return;
  ReaderChapterAnchor anchor;
  anchor.title = std::move(title);
  anchor.page = static_cast<int>(std::min<size_t>(line, static_cast<size_t>(INT_MAX)));
  anchor.scroll_y = static_cast<int>(std::min<size_t>(line * static_cast<size_t>(std::max(1, line_h)),
                                                      static_cast<size_t>(INT_MAX)));
  anchor.source_offset = source_offset;
  anchors.push_back(std::move(anchor));
}

std::vector<ReaderChapterAnchor> BuildRawTxtChapterAnchors(const TxtReaderState &state) {
  std::vector<ReaderChapterAnchor> anchors;
  if (state.pending_raw.empty()) return anchors;
  std::unordered_set<std::string> seen;
  size_t line_no = 0;
  size_t line_start = 0;
  size_t last_line = 0;
  bool have_last = false;

  for (size_t pos = 0; pos <= state.pending_raw.size(); ++pos) {
    const bool at_end = pos == state.pending_raw.size();
    if (!at_end && state.pending_raw[pos] != '\n' && state.pending_raw[pos] != '\r') continue;
    std::string line = state.pending_raw.substr(line_start, pos - line_start);
    if (LooksLikeChapterTitle(line) && (!have_last || HasEnoughRawGap(last_line, line_no))) {
      AddAnchor(anchors, seen, std::move(line), line_no, line_start, state.line_h);
      last_line = line_no;
      have_last = true;
      if (anchors.size() >= kMaxChapterAnchors) break;
    }
    if (!at_end && state.pending_raw[pos] == '\r' && pos + 1 < state.pending_raw.size() &&
        state.pending_raw[pos + 1] == '\n') {
      ++pos;
    }
    line_start = pos + 1;
    ++line_no;
  }

  if (line_no < kMinRawLinesForChapterList) anchors.clear();
  if (LooksLikeTocNoise(anchors, std::max(line_no, state.lines.size()))) anchors.clear();
  return anchors;
}

std::vector<ReaderChapterAnchor> BuildWrappedTxtChapterAnchors(const TxtReaderState &state) {
  std::vector<ReaderChapterAnchor> anchors;
  if (state.lines.size() < kMinRawLinesForChapterList) return anchors;
  std::unordered_set<std::string> seen;
  size_t last_line = 0;
  bool have_last = false;
  for (size_t i = 0; i < state.lines.size(); ++i) {
    std::string title = CollapseSpaces(Trim(state.lines[i]));
    if (!LooksLikeChapterTitle(title)) continue;
    if (have_last && !HasEnoughWrappedGap(state, last_line, i)) continue;
    const size_t source_offset = i < state.line_source_offsets.size() ? state.line_source_offsets[i] : 0;
    AddAnchor(anchors, seen, std::move(title), i, source_offset, state.line_h);
    last_line = i;
    have_last = true;
    if (anchors.size() >= kMaxChapterAnchors) break;
  }
  if (LooksLikeTocNoise(anchors, state.lines.size())) anchors.clear();
  return anchors;
}
}  // namespace

ReaderChapterAnchor MakeBodyChapterAnchor() {
  ReaderChapterAnchor anchor;
  anchor.title = u8"\u6b63\u6587";
  return anchor;
}

std::vector<ReaderChapterAnchor> BuildTxtChapterAnchors(const TxtReaderState &state) {
  std::vector<ReaderChapterAnchor> anchors;
  if (!state.loading) anchors = BuildRawTxtChapterAnchors(state);
  if (anchors.empty()) anchors = BuildWrappedTxtChapterAnchors(state);
  if (anchors.empty()) anchors.push_back(MakeBodyChapterAnchor());
  return anchors;
}

void ResetTxtChapterScan(ReaderUiState &ui, const TxtReaderState &state) {
  TxtChapterScanState scan;
  scan.cache_key = state.cache_key;
  scan.done = !state.loading && state.lines.empty();
  ui.txt_chapter_scan = std::move(scan);
}

bool TickTxtChapterScan(ReaderUiState &ui, const TxtReaderState &state, size_t line_budget) {
  TxtChapterScanState &scan = ui.txt_chapter_scan;
  if (!state.open) return false;
  if (scan.cache_key != state.cache_key) ResetTxtChapterScan(ui, state);
  if (scan.done && scan.scan_pos >= state.lines.size()) return false;

  const size_t before_count = scan.anchors.size();
  const bool was_done = scan.done;
  scan.done = false;
  size_t consumed = 0;
  while (scan.scan_pos < state.lines.size() && consumed < line_budget &&
         scan.anchors.size() < kMaxChapterAnchors) {
    const size_t line_index = scan.scan_pos++;
    ++consumed;
    std::string line = CollapseSpaces(Trim(state.lines[line_index]));
    if (line.empty()) continue;
    if (LooksLikeChapterTitle(line) &&
        (!scan.have_last_anchor || HasEnoughWrappedGap(state, scan.last_anchor_line, line_index))) {
      const size_t old_count = scan.anchors.size();
      const size_t source_offset = line_index < state.line_source_offsets.size()
                                       ? state.line_source_offsets[line_index]
                                       : 0;
      AddAnchor(scan.anchors, scan.seen_titles, std::move(line), line_index, source_offset, state.line_h);
      if (scan.anchors.size() != old_count) {
        scan.last_anchor_line = line_index;
        scan.have_last_anchor = true;
      }
    }
  }

  if (scan.anchors.size() >= kMaxChapterAnchors || (!state.loading && scan.scan_pos >= state.lines.size())) {
    scan.done = true;
  }
  if (scan.done) {
    if (state.lines.size() < kMinRawLinesForChapterList ||
        LooksLikeTocNoise(scan.anchors, state.lines.size())) {
      scan.anchors.clear();
      scan.seen_titles.clear();
      scan.have_last_anchor = false;
    }
  }
  return scan.anchors.size() != before_count || scan.done != was_done || consumed > 0;
}
