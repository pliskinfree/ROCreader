#include "epub_flow_reader.h"

#include "epub_runtime.h"
#include "async_image_render_queue.h"
#include "image_decode.h"
#include "chapter_detection.h"
#include "runtime_log.h"

#include <SDL.h>
#ifdef HAVE_SDL2_TTF
#include <SDL_ttf.h>
#endif

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include "filesystem_compat.h"
#include <limits>
#include <regex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#ifdef HAVE_LIBZIP
#include <zip.h>
#endif

namespace {

constexpr float kMinZoom = 0.7f;
constexpr float kMaxZoom = 2.4f;
constexpr float kZoomStep = 0.1f;
constexpr int kDefaultBaseFontPt = 18;
constexpr int kMargin = 14;
constexpr size_t kNaturalTextThreshold = 1200;
constexpr size_t kContentTextThreshold = 3000;
constexpr size_t kNaturalRunThreshold = 80;
constexpr size_t kLongNaturalRunThreshold = 180;
constexpr size_t kMinNaturalBlocks = 4;
constexpr size_t kMeaningfulParagraphThreshold = 24;
constexpr size_t kMinMeaningfulParagraphs = 20;
constexpr size_t kImageHeavyThreshold = 24;
constexpr size_t kMaxTextTextureCacheEntries = 768;
constexpr int kMaxImageTextureCreatesPerFrame = 1;
constexpr size_t kInitialFlowDocsToLoad = 6;
constexpr size_t kInitialFlowMinBlocks = 80;
constexpr size_t kMoreFlowDocsToLoad = 1;
constexpr int kLazyLoadAheadScreens = 1;
constexpr int64_t kFlowImageTexturePixelBudget = 3600000;

bool FlowImagePerfLogEnabled() {
  const char *env = std::getenv("ROCREADER_IMAGE_PERF_LOG");
  return env && *env && std::string(env) != "0";
}

void ApplyImageTextureFiltering(SDL_Texture *texture) {
#if SDL_VERSION_ATLEAST(2, 0, 12)
  if (texture) SDL_SetTextureScaleMode(texture, SDL_ScaleModeLinear);
#else
  (void)texture;
#endif
}

bool ResampleRgbaSurface(SDL_Surface *src, int dst_w, int dst_h, std::vector<unsigned char> &out,
                         std::atomic<bool> &cancel) {
  if (!src || src->w <= 0 || src->h <= 0 || dst_w <= 0 || dst_h <= 0) return false;
  out.assign(static_cast<size_t>(dst_w * dst_h * 4), 0);
  const bool area = dst_w < src->w || dst_h < src->h;
  for (int y = 0; y < dst_h; ++y) {
    if (cancel.load()) {
      out.clear();
      return false;
    }
    for (int x = 0; x < dst_w; ++x) {
      const size_t di = static_cast<size_t>((y * dst_w + x) * 4);
      if (area) {
        const double x0 = static_cast<double>(x) * src->w / dst_w;
        const double x1 = static_cast<double>(x + 1) * src->w / dst_w;
        const double y0 = static_cast<double>(y) * src->h / dst_h;
        const double y1 = static_cast<double>(y + 1) * src->h / dst_h;
        const int ix0 = std::max(0, static_cast<int>(std::floor(x0)));
        const int ix1 = std::min(src->w, static_cast<int>(std::ceil(x1)));
        const int iy0 = std::max(0, static_cast<int>(std::floor(y0)));
        const int iy1 = std::min(src->h, static_cast<int>(std::ceil(y1)));
        double sum[4] = {0.0, 0.0, 0.0, 0.0};
        double total = 0.0;
        for (int sy = iy0; sy < iy1; ++sy) {
          const double wy = std::max(0.0, std::min(y1, static_cast<double>(sy + 1)) -
                                              std::max(y0, static_cast<double>(sy)));
          if (wy <= 0.0) continue;
          const unsigned char *row = static_cast<const unsigned char *>(src->pixels) + sy * src->pitch;
          for (int sx = ix0; sx < ix1; ++sx) {
            const double wx = std::max(0.0, std::min(x1, static_cast<double>(sx + 1)) -
                                                std::max(x0, static_cast<double>(sx)));
            const double weight = wx * wy;
            if (weight <= 0.0) continue;
            const unsigned char *p = row + sx * 4;
            for (int c = 0; c < 4; ++c) sum[c] += static_cast<double>(p[c]) * weight;
            total += weight;
          }
        }
        if (total <= 0.0) total = 1.0;
        for (int c = 0; c < 4; ++c) {
          out[di + c] = static_cast<unsigned char>(std::clamp(static_cast<int>(std::lround(sum[c] / total)), 0, 255));
        }
      } else {
        const double sx = (static_cast<double>(x) + 0.5) * src->w / dst_w - 0.5;
        const double sy = (static_cast<double>(y) + 0.5) * src->h / dst_h - 0.5;
        const int x0 = std::clamp(static_cast<int>(std::floor(sx)), 0, src->w - 1);
        const int y0 = std::clamp(static_cast<int>(std::floor(sy)), 0, src->h - 1);
        const int x1 = std::min(x0 + 1, src->w - 1);
        const int y1 = std::min(y0 + 1, src->h - 1);
        const double fx = std::clamp(sx - std::floor(sx), 0.0, 1.0);
        const double fy = std::clamp(sy - std::floor(sy), 0.0, 1.0);
        const unsigned char *row0 = static_cast<const unsigned char *>(src->pixels) + y0 * src->pitch;
        const unsigned char *row1 = static_cast<const unsigned char *>(src->pixels) + y1 * src->pitch;
        const unsigned char *p00 = row0 + x0 * 4;
        const unsigned char *p10 = row0 + x1 * 4;
        const unsigned char *p01 = row1 + x0 * 4;
        const unsigned char *p11 = row1 + x1 * 4;
        for (int c = 0; c < 4; ++c) {
          const double top = p00[c] * (1.0 - fx) + p10[c] * fx;
          const double bottom = p01[c] * (1.0 - fx) + p11[c] * fx;
          out[di + c] =
              static_cast<unsigned char>(std::clamp(static_cast<int>(std::lround(top * (1.0 - fy) + bottom * fy)), 0, 255));
        }
      }
    }
  }
  return true;
}

enum class FlowBlockType { Paragraph, Header, ListItem, Image, Space };

struct FlowBlock {
  FlowBlockType type = FlowBlockType::Paragraph;
  std::string text;
  std::string doc_path;
  std::string resource;
  std::vector<std::string> lines;
  int heading_level = 0;
  int y = 0;
  int h = 0;
  int image_w = 0;
  int image_h = 0;
  int draw_w = 0;
  int draw_h = 0;
};

struct FlowDocAnchor {
  std::string doc_path;
  std::string title;
  size_t doc_index = 0;
  size_t block_index = std::numeric_limits<size_t>::max();
};

struct TextTextureEntry {
  SDL_Texture *texture = nullptr;
  int w = 0;
  int h = 0;
  uint32_t last_use = 0;
};

std::string FlowImageTextureKey(const std::string &resource, int target_w, int target_h) {
  return resource + "|" + std::to_string(std::max(1, target_w)) + "x" + std::to_string(std::max(1, target_h));
}

SDL_Point ClampFlowImageTarget(int source_w, int source_h, int target_w, int target_h) {
  target_w = std::max(1, target_w);
  target_h = std::max(1, target_h);
  if (source_w <= 0 || source_h <= 0) return SDL_Point{target_w, target_h};

  const double fit = std::min(static_cast<double>(target_w) / static_cast<double>(source_w),
                              static_cast<double>(target_h) / static_cast<double>(source_h));
  int out_w = std::max(1, static_cast<int>(std::llround(static_cast<double>(source_w) * fit)));
  int out_h = std::max(1, static_cast<int>(std::llround(static_cast<double>(source_h) * fit)));
  const int64_t pixels = static_cast<int64_t>(out_w) * static_cast<int64_t>(out_h);
  if (pixels > kFlowImageTexturePixelBudget && pixels > 0) {
    const double ratio = std::sqrt(static_cast<double>(kFlowImageTexturePixelBudget) / static_cast<double>(pixels));
    out_w = std::max(1, static_cast<int>(std::llround(static_cast<double>(out_w) * ratio)));
    out_h = std::max(1, static_cast<int>(std::llround(static_cast<double>(out_h) * ratio)));
  }
  return SDL_Point{out_w, out_h};
}

struct ManifestItem {
  std::string href;
  std::string media_type;
};

struct ParsedPackage {
  std::string opf_dir;
  std::unordered_map<std::string, ManifestItem> id_to_item;
  std::unordered_map<std::string, std::string> href_to_media_type;
  std::unordered_map<std::string, std::string> toc_titles_by_doc;
  std::vector<std::string> ordered_docs;
  std::vector<std::string> manifest_html_docs;
};

using AttrMap = std::unordered_map<std::string, std::string>;

std::string TagNameFromRaw(const std::string &raw, bool &closing);
bool IsBlockTag(const std::string &tag);

std::string NormalizeZipPath(std::string path) {
  std::replace(path.begin(), path.end(), '\\', '/');
  std::vector<std::string> parts;
  size_t cursor = 0;
  while (cursor <= path.size()) {
    const size_t slash = path.find('/', cursor);
    std::string part = path.substr(cursor, slash == std::string::npos ? std::string::npos : slash - cursor);
    if (part.empty() || part == ".") {
    } else if (part == "..") {
      if (!parts.empty()) parts.pop_back();
    } else {
      parts.push_back(std::move(part));
    }
    if (slash == std::string::npos) break;
    cursor = slash + 1;
  }
  std::string out;
  for (const auto &part : parts) {
    if (!out.empty()) out.push_back('/');
    out += part;
  }
  return out;
}

std::string ResolveRelative(const std::string &base_dir, std::string href) {
  const size_t hash = href.find('#');
  if (hash != std::string::npos) href.erase(hash);
  const size_t query = href.find('?');
  if (query != std::string::npos) href.erase(query);
  if (href.empty()) return NormalizeZipPath(base_dir);
  if (base_dir.empty()) return NormalizeZipPath(href);
  return NormalizeZipPath(base_dir + "/" + href);
}

std::string LowerAscii(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return s;
}

std::string TrimSpaces(const std::string &s) {
  size_t first = 0;
  while (first < s.size() && std::isspace(static_cast<unsigned char>(s[first]))) ++first;
  size_t last = s.size();
  while (last > first && std::isspace(static_cast<unsigned char>(s[last - 1]))) --last;
  return s.substr(first, last - first);
}

std::string DecodeHtmlEntities(std::string s) {
  const std::pair<const char *, const char *> table[] = {
      {"&nbsp;", " "}, {"&lt;", "<"}, {"&gt;", ">"}, {"&amp;", "&"},
      {"&quot;", "\""}, {"&apos;", "'"},
  };
  for (const auto &kv : table) {
    size_t pos = 0;
    while ((pos = s.find(kv.first, pos)) != std::string::npos) {
      s.replace(pos, std::strlen(kv.first), kv.second);
      pos += std::strlen(kv.second);
    }
  }
  return s;
}

AttrMap ParseTagAttrs(const std::string &attrs_raw) {
  AttrMap attrs;
  const std::regex attr_re("([A-Za-z_:][-A-Za-z0-9_:.]*)\\s*=\\s*(['\"])(.*?)\\2", std::regex::icase);
  for (std::sregex_iterator it(attrs_raw.begin(), attrs_raw.end(), attr_re), end; it != end; ++it) {
    attrs[LowerAscii((*it)[1].str())] = DecodeHtmlEntities((*it)[3].str());
  }
  return attrs;
}

bool PickFirstMatch(const std::string &src, const std::regex &re, std::string &out) {
  std::smatch m;
  if (!std::regex_search(src, m, re) || m.size() < 2) return false;
  out = m[1].str();
  return true;
}

bool IsHtmlMediaType(const std::string &media_type) {
  return media_type == "application/xhtml+xml" || media_type == "text/html";
}

size_t Utf8CharLen(unsigned char c);
std::string StripTagsToText(std::string html);
#ifdef HAVE_LIBZIP
bool ReadZipEntry(zip_t *za, const std::string &name, std::string &out);
#endif

std::string TruncateUtf8Bytes(std::string text, size_t max_bytes) {
  if (text.size() <= max_bytes) return text;
  size_t end = 0;
  while (end < text.size()) {
    const size_t len = Utf8CharLen(static_cast<unsigned char>(text[end]));
    if (end + len > max_bytes) break;
    end += len;
  }
  text.resize(end);
  return text;
}

#ifdef HAVE_LIBZIP
void ParseNcxToc(zip_t *za, const std::string &ncx_path, ParsedPackage &pkg) {
  std::string ncx;
  if (!za || ncx_path.empty() || !ReadZipEntry(za, ncx_path, ncx)) return;

  const std::string ncx_dir = std::filesystem::path(ncx_path).parent_path().generic_string();
  const std::regex nav_point_re("<navPoint\\b[^>]*>([\\s\\S]*?)</navPoint>", std::regex::icase);
  const std::regex label_re("<navLabel\\b[^>]*>[\\s\\S]*?<text\\b[^>]*>([\\s\\S]*?)</text>[\\s\\S]*?</navLabel>",
                            std::regex::icase);
  const std::regex content_re("<content\\b([^>]*)>", std::regex::icase);

  size_t parsed = 0;
  for (std::sregex_iterator it(ncx.begin(), ncx.end(), nav_point_re), end; it != end; ++it) {
    const std::string nav_point = (*it)[1].str();
    std::string raw_label;
    std::string content_attrs_raw;
    if (!PickFirstMatch(nav_point, label_re, raw_label) ||
        !PickFirstMatch(nav_point, content_re, content_attrs_raw)) {
      continue;
    }
    AttrMap content_attrs = ParseTagAttrs(content_attrs_raw);
    const auto src_it = content_attrs.find("src");
    if (src_it == content_attrs.end() || src_it->second.empty()) continue;

    std::string title = TruncateUtf8Bytes(TrimSpaces(StripTagsToText(raw_label)), 96);
    if (title.empty()) continue;
    const std::string doc = ResolveRelative(ncx_dir, src_it->second);
    if (doc.empty()) continue;
    pkg.toc_titles_by_doc.emplace(doc, std::move(title));
    if (++parsed >= 1024) break;
  }
}
#endif

bool IsLikelyContentCodepoint(uint32_t cp) {
  return (cp >= 0x4E00 && cp <= 0x9FFF) ||
         (cp >= 0x3400 && cp <= 0x4DBF) ||
         (cp >= 'A' && cp <= 'Z') ||
         (cp >= 'a' && cp <= 'z') ||
         (cp >= '0' && cp <= '9');
}

uint32_t DecodeUtf8Codepoint(const std::string &s, size_t i, size_t len) {
  const unsigned char c0 = static_cast<unsigned char>(s[i]);
  if (len == 1) return c0;
  if (len == 2 && i + 1 < s.size()) {
    return ((c0 & 0x1Fu) << 6) | (static_cast<unsigned char>(s[i + 1]) & 0x3Fu);
  }
  if (len == 3 && i + 2 < s.size()) {
    return ((c0 & 0x0Fu) << 12) |
           ((static_cast<unsigned char>(s[i + 1]) & 0x3Fu) << 6) |
           (static_cast<unsigned char>(s[i + 2]) & 0x3Fu);
  }
  if (len == 4 && i + 3 < s.size()) {
    return ((c0 & 0x07u) << 18) |
           ((static_cast<unsigned char>(s[i + 1]) & 0x3Fu) << 12) |
           ((static_cast<unsigned char>(s[i + 2]) & 0x3Fu) << 6) |
           (static_cast<unsigned char>(s[i + 3]) & 0x3Fu);
  }
  return c0;
}

size_t Utf8CharLen(unsigned char c) {
  if ((c & 0x80) == 0) return 1;
  if ((c & 0xE0) == 0xC0) return 2;
  if ((c & 0xF0) == 0xE0) return 3;
  if ((c & 0xF8) == 0xF0) return 4;
  return 1;
}

std::vector<std::string> WrapUtf8Text(const std::string &text, int chars_per_line) {
  std::vector<std::string> lines;
  std::string line;
  int count = 0;
  for (size_t i = 0; i < text.size();) {
    const size_t len = std::min(Utf8CharLen(static_cast<unsigned char>(text[i])), text.size() - i);
    const std::string ch = text.substr(i, len);
    const bool hard_break = (ch == "\n");
    if (hard_break || count >= chars_per_line) {
      std::string trimmed = TrimSpaces(line);
      if (!trimmed.empty()) lines.push_back(std::move(trimmed));
      line.clear();
      count = 0;
      if (hard_break) {
        i += len;
        continue;
      }
    }
    line += ch;
    if (!std::isspace(static_cast<unsigned char>(text[i]))) ++count;
    i += len;
  }
  std::string trimmed = TrimSpaces(line);
  if (!trimmed.empty()) lines.push_back(std::move(trimmed));
  if (lines.empty()) lines.push_back("");
  return lines;
}

struct NaturalTextStats {
  size_t natural_chars = 0;
  size_t natural_blocks = 0;
  size_t long_natural_blocks = 0;
  size_t content_chars = 0;
  size_t meaningful_paragraphs = 0;
  size_t max_run = 0;
};

NaturalTextStats MeasureNaturalText(const std::string &text) {
  NaturalTextStats stats;
  size_t run = 0;
  size_t paragraph_content = 0;
  auto finish_run = [&]() {
    stats.max_run = std::max(stats.max_run, run);
    stats.content_chars += run;
    paragraph_content += run;
    if (run >= kNaturalRunThreshold) {
      stats.natural_chars += run;
      ++stats.natural_blocks;
      if (run >= kLongNaturalRunThreshold) ++stats.long_natural_blocks;
    }
    run = 0;
  };
  auto finish_paragraph = [&]() {
    finish_run();
    if (paragraph_content >= kMeaningfulParagraphThreshold) ++stats.meaningful_paragraphs;
    paragraph_content = 0;
  };
  for (size_t i = 0; i < text.size();) {
    const size_t len = std::min(Utf8CharLen(static_cast<unsigned char>(text[i])), text.size() - i);
    const uint32_t cp = DecodeUtf8Codepoint(text, i, len);
    if (IsLikelyContentCodepoint(cp)) {
      ++run;
    } else if (cp == '\n') {
      finish_paragraph();
    } else if (cp == 0x3002 || cp == 0xFF0C || cp == 0xFF1B || cp == '.' || cp == ',' || cp == ';' ||
               std::isspace(static_cast<unsigned char>(text[i]))) {
      if (run > 0 && run < kNaturalRunThreshold && cp != '\n') {
        // keep sentence punctuation inside a candidate paragraph run
      } else {
        finish_run();
      }
    } else {
      finish_run();
    }
    i += len;
  }
  finish_paragraph();
  return stats;
}

size_t CountImgTagsLinear(const std::string &html) {
  size_t count = 0;
  size_t cursor = 0;
  while (cursor < html.size()) {
    const size_t lt = html.find('<', cursor);
    if (lt == std::string::npos) break;
    const size_t gt = html.find('>', lt + 1);
    if (gt == std::string::npos) break;
    bool closing = false;
    if (TagNameFromRaw(html.substr(lt + 1, gt - lt - 1), closing) == "img" && !closing) ++count;
    cursor = gt + 1;
  }
  return count;
}

bool ReadBigEndian16(const unsigned char *p, size_t size, size_t off, int &v) {
  if (off + 1 >= size) return false;
  v = (static_cast<int>(p[off]) << 8) | static_cast<int>(p[off + 1]);
  return true;
}

bool ReadBigEndian32(const unsigned char *p, size_t size, size_t off, int &v) {
  if (off + 3 >= size) return false;
  v = (static_cast<int>(p[off]) << 24) | (static_cast<int>(p[off + 1]) << 16) |
      (static_cast<int>(p[off + 2]) << 8) | static_cast<int>(p[off + 3]);
  return true;
}

bool ReadLittleEndian16(const unsigned char *p, size_t size, size_t off, int &v) {
  if (off + 1 >= size) return false;
  v = static_cast<int>(p[off]) | (static_cast<int>(p[off + 1]) << 8);
  return true;
}

bool ProbeImageSizeFromBytes(const std::string &bytes, int &w, int &h) {
  w = 0;
  h = 0;
  const auto *p = reinterpret_cast<const unsigned char *>(bytes.data());
  const size_t n = bytes.size();
  if (n >= 24 && std::memcmp(p, "\x89PNG\r\n\x1a\n", 8) == 0) {
    return ReadBigEndian32(p, n, 16, w) && ReadBigEndian32(p, n, 20, h) && w > 0 && h > 0;
  }
  if (n >= 10 && std::memcmp(p, "GIF", 3) == 0) {
    return ReadLittleEndian16(p, n, 6, w) && ReadLittleEndian16(p, n, 8, h) && w > 0 && h > 0;
  }
  if (n >= 30 && std::memcmp(p, "RIFF", 4) == 0 && std::memcmp(p + 8, "WEBP", 4) == 0) {
    if (std::memcmp(p + 12, "VP8X", 4) == 0 && n >= 30) {
      w = 1 + static_cast<int>(p[24]) + (static_cast<int>(p[25]) << 8) + (static_cast<int>(p[26]) << 16);
      h = 1 + static_cast<int>(p[27]) + (static_cast<int>(p[28]) << 8) + (static_cast<int>(p[29]) << 16);
      return w > 0 && h > 0;
    }
    if (std::memcmp(p + 12, "VP8 ", 4) == 0 && n >= 30) {
      return ReadLittleEndian16(p, n, 26, w) && ReadLittleEndian16(p, n, 28, h) && w > 0 && h > 0;
    }
  }
  if (n >= 4 && p[0] == 0xFF && p[1] == 0xD8) {
    size_t off = 2;
    while (off + 9 < n) {
      if (p[off] != 0xFF) {
        ++off;
        continue;
      }
      while (off < n && p[off] == 0xFF) ++off;
      if (off >= n) break;
      const unsigned char marker = p[off++];
      if (marker == 0xD8 || marker == 0xD9) continue;
      int len = 0;
      if (!ReadBigEndian16(p, n, off, len) || len < 2 || off + static_cast<size_t>(len) > n) break;
      if ((marker >= 0xC0 && marker <= 0xC3) || (marker >= 0xC5 && marker <= 0xC7) ||
          (marker >= 0xC9 && marker <= 0xCB) || (marker >= 0xCD && marker <= 0xCF)) {
        return ReadBigEndian16(p, n, off + 3, h) && ReadBigEndian16(p, n, off + 5, w) && w > 0 && h > 0;
      }
      off += static_cast<size_t>(len);
    }
  }
  return false;
}

#ifdef HAVE_LIBZIP
bool ReadZipEntry(zip_t *za, const std::string &name, std::string &out) {
  zip_stat_t st{};
  zip_int64_t index = zip_name_locate(za, name.c_str(), 0);
  if (index < 0) index = zip_name_locate(za, name.c_str(), ZIP_FL_NOCASE);
  if (index < 0) return false;
  if (zip_stat_index(za, static_cast<zip_uint64_t>(index), 0, &st) != 0) return false;
  if (st.size > static_cast<zip_uint64_t>(128 * 1024 * 1024)) return false;
  zip_file_t *zf = zip_fopen_index(za, static_cast<zip_uint64_t>(index), 0);
  if (!zf) return false;
  out.resize(static_cast<size_t>(st.size));
  size_t total = 0;
  while (total < out.size()) {
    const zip_int64_t rd = zip_fread(zf, out.data() + total, out.size() - total);
    if (rd < 0) {
      zip_fclose(zf);
      out.clear();
      return false;
    }
    if (rd == 0) break;
    total += static_cast<size_t>(rd);
  }
  zip_fclose(zf);
  out.resize(total);
  return !out.empty();
}

bool ReadZipEntryPrefix(zip_t *za, const std::string &name, size_t max_bytes, std::string &out) {
  out.clear();
  if (max_bytes == 0) return false;
  zip_int64_t index = zip_name_locate(za, name.c_str(), 0);
  if (index < 0) index = zip_name_locate(za, name.c_str(), ZIP_FL_NOCASE);
  if (index < 0) return false;
  zip_file_t *zf = zip_fopen_index(za, static_cast<zip_uint64_t>(index), 0);
  if (!zf) return false;
  out.resize(max_bytes);
  const zip_int64_t rd = zip_fread(zf, out.data(), out.size());
  zip_fclose(zf);
  if (rd <= 0) {
    out.clear();
    return false;
  }
  out.resize(static_cast<size_t>(rd));
  return true;
}

bool ParsePackage(zip_t *za, ParsedPackage &pkg, std::string &error) {
  std::string container_xml;
  if (!ReadZipEntry(za, "META-INF/container.xml", container_xml)) {
    error = "missing META-INF/container.xml";
    return false;
  }
  std::string rootfile_tag;
  if (!PickFirstMatch(container_xml, std::regex("<rootfile\\b([^>]*)>", std::regex::icase), rootfile_tag)) {
    error = "cannot locate rootfile";
    return false;
  }
  AttrMap root_attrs = ParseTagAttrs(rootfile_tag);
  const auto full_it = root_attrs.find("full-path");
  if (full_it == root_attrs.end() || full_it->second.empty()) {
    error = "rootfile full-path missing";
    return false;
  }
  const std::string opf_path = full_it->second;
  std::string opf;
  if (!ReadZipEntry(za, opf_path, opf)) {
    error = "cannot read OPF";
    return false;
  }
  try {
    pkg.opf_dir = std::filesystem::path(opf_path).parent_path().generic_string();
  } catch (...) {
    pkg.opf_dir.clear();
  }

  const std::regex item_re("<item\\b([^>]*)>", std::regex::icase);
  std::string ncx_path;
  for (std::sregex_iterator it(opf.begin(), opf.end(), item_re), end; it != end; ++it) {
    AttrMap attrs = ParseTagAttrs((*it)[1].str());
    const auto id_it = attrs.find("id");
    const auto href_it = attrs.find("href");
    if (id_it == attrs.end() || href_it == attrs.end() || id_it->second.empty() || href_it->second.empty()) continue;
    ManifestItem item;
    item.href = ResolveRelative(pkg.opf_dir, href_it->second);
    const auto mt_it = attrs.find("media-type");
    if (mt_it != attrs.end()) item.media_type = mt_it->second;
    pkg.id_to_item[id_it->second] = item;
    pkg.href_to_media_type[item.href] = item.media_type;
    if (IsHtmlMediaType(item.media_type)) pkg.manifest_html_docs.push_back(item.href);
    if (item.media_type == "application/x-dtbncx+xml" || LowerAscii(id_it->second).find("ncx") != std::string::npos) {
      ncx_path = item.href;
    }
  }

  const std::regex spine_re("<itemref\\b[^>]*idref\\s*=\\s*['\"]([^'\"]+)['\"][^>]*>", std::regex::icase);
  for (std::sregex_iterator it(opf.begin(), opf.end(), spine_re), end; it != end; ++it) {
    const auto mit = pkg.id_to_item.find((*it)[1].str());
    if (mit == pkg.id_to_item.end() || !IsHtmlMediaType(mit->second.media_type)) continue;
    pkg.ordered_docs.push_back(mit->second.href);
  }
  if (pkg.ordered_docs.empty()) pkg.ordered_docs = pkg.manifest_html_docs;
  if (pkg.ordered_docs.empty()) {
    error = "no html/xhtml spine content";
    return false;
  }
  ParseNcxToc(za, ncx_path, pkg);
  return true;
}
#endif

std::string StripTagsToText(std::string html) {
  std::string out;
  out.reserve(html.size());
  bool in_tag = false;
  bool last_space = true;
  for (size_t i = 0; i < html.size();) {
    const char ch = html[i];
    if (ch == '<') {
      const size_t gt = html.find('>', i + 1);
      if (gt == std::string::npos) break;
      const std::string raw = html.substr(i + 1, gt - i - 1);
      bool closing = false;
      const std::string tag = TagNameFromRaw(raw, closing);
      if (!closing && (tag == "script" || tag == "style")) {
        const std::string lower_tail = LowerAscii(html.substr(gt + 1));
        const std::string close_pat = "</" + tag;
        const size_t close_pos = lower_tail.find(close_pat);
        if (close_pos != std::string::npos) {
          const size_t close_start = gt + 1 + close_pos;
          const size_t close_end = html.find('>', close_start);
          i = (close_end == std::string::npos) ? html.size() : close_end + 1;
          continue;
        }
      }
      if (tag == "br" || IsBlockTag(tag)) {
        if (!out.empty() && out.back() != '\n') out.push_back('\n');
        last_space = true;
      } else if (!last_space) {
        out.push_back(' ');
        last_space = true;
      }
      i = gt + 1;
      continue;
    }
    in_tag = false;
    (void)in_tag;
    if (ch == '&') {
      const size_t semi = html.find(';', i + 1);
      if (semi != std::string::npos && semi - i <= 12) {
        std::string entity = DecodeHtmlEntities(html.substr(i, semi - i + 1));
        if (entity.size() < semi - i + 1) {
          for (char ec : entity) {
            if (std::isspace(static_cast<unsigned char>(ec))) {
              if (!last_space) out.push_back(' ');
              last_space = true;
            } else {
              out.push_back(ec);
              last_space = false;
            }
          }
          i = semi + 1;
          continue;
        }
      }
    }
    const size_t len = std::min(Utf8CharLen(static_cast<unsigned char>(ch)), html.size() - i);
    if (std::isspace(static_cast<unsigned char>(ch))) {
      if (ch == '\n') {
        if (!out.empty() && out.back() != '\n') out.push_back('\n');
      } else if (!last_space) {
        out.push_back(' ');
      }
      last_space = true;
    } else {
      out.append(html, i, len);
      last_space = false;
    }
    i += len;
  }
  return TrimSpaces(out);
}

void PushTextBlock(std::vector<FlowBlock> &blocks, const std::string &doc_path,
                   FlowBlockType type, std::string text, int heading_level = 0) {
  text = StripTagsToText(std::move(text));
  if (text.empty()) return;
  FlowBlock block;
  block.type = type;
  block.doc_path = doc_path;
  block.text = std::move(text);
  block.heading_level = heading_level;
  blocks.push_back(std::move(block));
}

void PushImageBlock(std::vector<FlowBlock> &blocks, const std::string &doc_base, const std::string &attrs_raw) {
  AttrMap attrs = ParseTagAttrs(attrs_raw);
  const auto src_it = attrs.find("src");
  if (src_it == attrs.end() || src_it->second.empty()) return;
  FlowBlock block;
  block.type = FlowBlockType::Image;
  block.doc_path = doc_base;
  block.resource = ResolveRelative(doc_base, src_it->second);
  blocks.push_back(std::move(block));
}

std::string TagNameFromRaw(const std::string &raw, bool &closing) {
  closing = false;
  size_t i = 0;
  while (i < raw.size() && std::isspace(static_cast<unsigned char>(raw[i]))) ++i;
  if (i < raw.size() && raw[i] == '/') {
    closing = true;
    ++i;
    while (i < raw.size() && std::isspace(static_cast<unsigned char>(raw[i]))) ++i;
  }
  const size_t start = i;
  while (i < raw.size() && std::isalnum(static_cast<unsigned char>(raw[i]))) ++i;
  return LowerAscii(raw.substr(start, i - start));
}

std::string AttrsRawFromTagRaw(const std::string &raw) {
  size_t i = 0;
  while (i < raw.size() && std::isspace(static_cast<unsigned char>(raw[i]))) ++i;
  if (i < raw.size() && raw[i] == '/') ++i;
  while (i < raw.size() && std::isspace(static_cast<unsigned char>(raw[i]))) ++i;
  while (i < raw.size() && std::isalnum(static_cast<unsigned char>(raw[i]))) ++i;
  return raw.substr(i);
}

bool IsBlockTag(const std::string &tag) {
  return tag == "p" || tag == "div" || tag == "li" ||
         (tag.size() == 2 && tag[0] == 'h' && tag[1] >= '1' && tag[1] <= '6');
}

FlowBlockType TypeForTag(const std::string &tag) {
  if (tag == "li") return FlowBlockType::ListItem;
  if (tag.size() == 2 && tag[0] == 'h' && tag[1] >= '1' && tag[1] <= '6') return FlowBlockType::Header;
  return FlowBlockType::Paragraph;
}

int HeadingLevelForTag(const std::string &tag) {
  if (tag.size() == 2 && tag[0] == 'h' && tag[1] >= '1' && tag[1] <= '6') return tag[1] - '0';
  return 0;
}

void FlushFlowText(std::vector<FlowBlock> &blocks,
                   const std::string &doc_path,
                   std::string &text,
                   FlowBlockType type,
                   int heading_level) {
  PushTextBlock(blocks, doc_path, type, text, heading_level);
  text.clear();
}

void ParseHtmlBlocks(const std::string &html, const std::string &doc_path, std::vector<FlowBlock> &blocks) {
  const std::string doc_base = std::filesystem::path(doc_path).parent_path().generic_string();
  FlowBlockType current_type = FlowBlockType::Paragraph;
  int current_heading = 0;
  std::string text;
  size_t cursor = 0;
  while (cursor < html.size()) {
    const size_t lt = html.find('<', cursor);
    if (lt == std::string::npos) {
      text.append(html, cursor, std::string::npos);
      break;
    }
    text.append(html, cursor, lt - cursor);
    const size_t gt = html.find('>', lt + 1);
    if (gt == std::string::npos) break;
    const std::string raw = html.substr(lt + 1, gt - lt - 1);
    bool closing = false;
    const std::string tag = TagNameFromRaw(raw, closing);
    const std::string attrs_raw = AttrsRawFromTagRaw(raw);

    if (!closing && (tag == "script" || tag == "style")) {
      const std::string lower_tail = LowerAscii(html.substr(gt + 1));
      const std::string close_pat = "</" + tag;
      const size_t close_pos = lower_tail.find(close_pat);
      if (close_pos != std::string::npos) {
        const size_t close_start = gt + 1 + close_pos;
        const size_t close_end = html.find('>', close_start);
        cursor = (close_end == std::string::npos) ? html.size() : close_end + 1;
        continue;
      }
    }

    if (!closing && tag == "br") {
      text.push_back('\n');
    } else if (!closing && tag == "img") {
      FlushFlowText(blocks, doc_path, text, current_type, current_heading);
      PushImageBlock(blocks, doc_base, attrs_raw);
    } else if (!closing && IsBlockTag(tag)) {
      FlushFlowText(blocks, doc_path, text, current_type, current_heading);
      current_type = TypeForTag(tag);
      current_heading = HeadingLevelForTag(tag);
    } else if (closing && IsBlockTag(tag)) {
      FlushFlowText(blocks, doc_path, text, current_type, current_heading);
      current_type = FlowBlockType::Paragraph;
      current_heading = 0;
    }
    cursor = gt + 1;
  }
  FlushFlowText(blocks, doc_path, text, current_type, current_heading);
}

std::string TitleFromHtml(const std::string &html, const std::string &fallback) {
  std::string title;
  static const std::regex heading_re("<h[1-3]\\b[^>]*>(.*?)</h[1-3]>", std::regex::icase);
  static const std::regex title_re("<title\\b[^>]*>(.*?)</title>", std::regex::icase);
  if (!PickFirstMatch(html, heading_re, title)) PickFirstMatch(html, title_re, title);
  title = TrimSpaces(StripTagsToText(title));
  if (title.empty()) title = fallback;
  if (title.size() > 96) title.resize(96);
  return title;
}

int NormalizeRotation(int rotation) {
  rotation %= 360;
  if (rotation < 0) rotation += 360;
  return rotation;
}

bool RenderFlowImageJob(const std::string &epub_path, const AsyncImageRenderJob &job,
                        std::atomic<bool> &cancel, AsyncImageRenderResult &out) {
#ifdef HAVE_LIBZIP
  if (epub_path.empty() || job.source_key.empty() || cancel.load()) return false;
  int zerr = 0;
  zip_t *za = zip_open(epub_path.c_str(), ZIP_RDONLY, &zerr);
  if (!za) return false;
  std::string bytes;
  const bool ok = ReadZipEntry(za, job.source_key, bytes);
  zip_close(za);
  if (!ok || bytes.empty() || cancel.load()) return false;

  SDL_Surface *surface = DecodeSurfaceFromMemoryFit(bytes.data(), bytes.size(), job.viewport_w, job.viewport_h);
  if (!surface || cancel.load()) {
    if (surface) SDL_FreeSurface(surface);
    return false;
  }

  SDL_Surface *rgba_surface = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGBA32, 0);
  SDL_FreeSurface(surface);
  if (!rgba_surface || cancel.load()) {
    if (rgba_surface) SDL_FreeSurface(rgba_surface);
    return false;
  }

  out.width = rgba_surface->w;
  out.height = rgba_surface->h;
  if (job.viewport_w > 0 && job.viewport_h > 0 &&
      (rgba_surface->w > job.viewport_w || rgba_surface->h > job.viewport_h)) {
    out.width = job.viewport_w;
    out.height = job.viewport_h;
  }
  if (!ResampleRgbaSurface(rgba_surface, out.width, out.height, out.rgba, cancel)) {
    SDL_FreeSurface(rgba_surface);
    return false;
  }
  SDL_FreeSurface(rgba_surface);
  return !cancel.load() && out.width > 0 && out.height > 0 && !out.rgba.empty();
#else
  (void)epub_path;
  (void)job;
  (void)cancel;
  (void)out;
  return false;
#endif
}

}  // namespace

struct EpubFlowReader::Impl {
  SDL_Renderer *renderer = nullptr;
  std::string path;
  int screen_w = 720;
  int screen_h = 480;
  int base_font_pt = kDefaultBaseFontPt;
  SDL_Color background_color{250, 249, 244, 255};
  SDL_Color font_color{28, 28, 28, 255};
  int rotation = 0;
  float zoom = 1.0f;
  int scroll_y = 0;
  int doc_h = 1;
  int layout_width = 0;
  int layout_image_width = 0;
  int layout_font_pt = 0;
  std::vector<std::string> ordered_docs;
  size_t next_doc_index = 0;
  bool all_docs_loaded = true;
  bool lazy_load_requested = false;
  std::vector<FlowBlock> source_blocks;
  std::vector<FlowDocAnchor> doc_anchors;
  std::unordered_map<std::string, std::string> toc_titles_by_doc;
  mutable AsyncImageRenderQueue image_queue;
  mutable std::unordered_map<std::string, SDL_Texture *> image_textures;
  mutable std::unordered_map<std::string, SDL_Point> image_natural_sizes;
  mutable std::unordered_map<std::string, SDL_Point> image_texture_sizes;
  mutable std::unordered_map<std::string, AsyncImageRenderResult> image_ready;
  mutable std::unordered_set<std::string> image_pending;
  mutable std::unordered_map<std::string, TextTextureEntry> text_textures;
  mutable int image_texture_creates_this_frame = 0;
  mutable int image_decode_requests_this_frame = 0;
#ifdef HAVE_SDL2_TTF
  TTF_Font *font = nullptr;
  TTF_Font *header_font = nullptr;
#endif

  int LayoutWidth() const {
    const int w = (rotation == 90 || rotation == 270) ? screen_h : screen_w;
    return std::max(80, w - kMargin * 2);
  }

  int ViewWidth() const {
    return std::max(1, (rotation == 90 || rotation == 270) ? screen_h : screen_w);
  }

  int ViewHeight() const {
    return std::max(1, (rotation == 90 || rotation == 270) ? screen_w : screen_h);
  }

  int FontPt() const { return std::max(8, static_cast<int>(std::lround(base_font_pt * zoom))); }

  bool FontHasChineseGlyph(const std::string &path, int pt) const {
#ifdef HAVE_SDL2_TTF
    TTF_Font *probe = TTF_OpenFont(path.c_str(), std::max(8, pt));
    if (!probe) return false;
    const bool ok = TTF_GlyphIsProvided(probe, 0x660E) != 0 &&
                    TTF_GlyphIsProvided(probe, 0x671D) != 0;
    TTF_CloseFont(probe);
    return ok;
#else
    (void)path;
    (void)pt;
    return false;
#endif
  }

  std::string FontPath() const {
    const std::vector<std::string> candidates = {
        "fonts/ui_font_02.ttf", "resources/fonts/ui_font_02.ttf",
        "fonts/ui_font.ttf", "resources/fonts/ui_font.ttf",
        "resources/fonts/default.ttf", "default.ttf",
        "/Roms/APPS/ROCreader/fonts/ui_font_02.ttf",
        "/Roms/APPS/ROCreader/fonts/ui_font.ttf",
        "/mnt/mmc/ROCreader/fonts/ui_font_02.ttf",
        "/mnt/mmc/ROCreader/fonts/ui_font.ttf",
        "/mnt/mmc/Roms/ROCreader/fonts/ui_font_02.ttf",
        "/mnt/mmc/Roms/ROCreader/fonts/ui_font.ttf",
        "/mnt/mmc2/ROCreader/fonts/ui_font_02.ttf",
        "/mnt/mmc2/ROCreader/fonts/ui_font.ttf",
        "/mnt/mmc2/Roms/ROCreader/fonts/ui_font_02.ttf",
        "/mnt/mmc2/Roms/ROCreader/fonts/ui_font.ttf",
    };
    std::string fallback;
    for (const auto &candidate : candidates) {
      std::error_code ec;
      if (!std::filesystem::exists(candidate, ec) || ec) continue;
      if (fallback.empty()) fallback = candidate;
      if (FontHasChineseGlyph(candidate, FontPt())) return candidate;
    }
    return fallback;
  }

  void CloseFonts() {
#ifdef HAVE_SDL2_TTF
    if (font) TTF_CloseFont(font);
    if (header_font) TTF_CloseFont(header_font);
    font = nullptr;
    header_font = nullptr;
#endif
  }

  void EnsureFonts() {
#ifdef HAVE_SDL2_TTF
    const std::string fp = FontPath();
    if (fp.empty()) return;
    const int body_pt = FontPt();
    if (!font) font = TTF_OpenFont(fp.c_str(), body_pt);
    if (!header_font) header_font = TTF_OpenFont(fp.c_str(), std::max(body_pt + 4, static_cast<int>(body_pt * 1.25f)));
    runtime_log::Line("[epub_flow] font path=" + fp +
                      " chinese=" + (FontHasChineseGlyph(fp, body_pt) ? "yes" : "no"));
#endif
  }

  void DestroyImages(bool clear_natural_sizes = false) const {
    image_queue.CancelTarget();
    image_pending.clear();
    image_ready.clear();
    image_decode_requests_this_frame = 0;
    for (auto &kv : image_textures) {
      if (kv.second) SDL_DestroyTexture(kv.second);
    }
    image_textures.clear();
    image_texture_sizes.clear();
    if (clear_natural_sizes) image_natural_sizes.clear();
  }

  void DestroyTextTextures() const {
    for (auto &kv : text_textures) {
      if (kv.second.texture) SDL_DestroyTexture(kv.second.texture);
    }
    text_textures.clear();
  }

  void PruneTextTextures() const {
    while (text_textures.size() > kMaxTextTextureCacheEntries) {
      auto oldest = text_textures.end();
      for (auto it = text_textures.begin(); it != text_textures.end(); ++it) {
        if (oldest == text_textures.end() || it->second.last_use < oldest->second.last_use) oldest = it;
      }
      if (oldest == text_textures.end()) break;
      if (oldest->second.texture) SDL_DestroyTexture(oldest->second.texture);
      text_textures.erase(oldest);
    }
  }

  bool ReadImageSize(const std::string &resource, int &w, int &h) {
    w = 0;
    h = 0;
    const auto cached = image_natural_sizes.find(resource);
    if (cached != image_natural_sizes.end()) {
      w = cached->second.x;
      h = cached->second.y;
      return w > 0 && h > 0;
    }
#ifdef HAVE_LIBZIP
    int zerr = 0;
    zip_t *za = zip_open(path.c_str(), ZIP_RDONLY, &zerr);
    if (!za) return false;
    std::string bytes;
    const bool ok = ReadZipEntryPrefix(za, resource, 64 * 1024, bytes);
    zip_close(za);
    if (!ok || bytes.empty()) return false;
    if (ProbeImageSizeFromBytes(bytes, w, h)) {
      image_natural_sizes[resource] = SDL_Point{w, h};
      return true;
    }
    return false;
#else
    (void)resource;
    return false;
#endif
  }

  void StartImageQueue() {
    image_queue.Shutdown();
    image_queue.Start("epub_flow_image_worker",
                      [this](const AsyncImageRenderJob &job, std::atomic<bool> &cancel,
                             AsyncImageRenderResult &out) {
                        return RenderFlowImageJob(path, job, cancel, out);
                      });
  }

  void Relayout(bool clear_images = true, bool clear_text_cache = true) {
    if (clear_images) DestroyImages();
    if (clear_text_cache) {
      DestroyTextTextures();
      CloseFonts();
      EnsureFonts();
    } else if (!font || !header_font) {
      EnsureFonts();
    }
    LayoutBlockRange(0, kMargin);
    scroll_y = std::clamp(scroll_y, 0, MaxScroll());
  }

  int MaxScroll() const { return std::max(0, doc_h - ViewHeight()); }

  void LayoutBlockRange(size_t first, int start_y) {
    const int width = LayoutWidth();
    const int image_width = ViewWidth();
    const int line_h = std::max(16, static_cast<int>(std::lround(FontPt() * 1.45f)));
    int y = start_y;
    for (size_t i = first; i < source_blocks.size(); ++i) {
      FlowBlock &block = source_blocks[i];
      block.y = y;
      block.h = 0;
      if (block.type == FlowBlockType::Image) {
        int iw = 0;
        int ih = 0;
        block.draw_w = image_width;
        if (ReadImageSize(block.resource, iw, ih)) {
          block.image_w = iw;
          block.image_h = ih;
          block.draw_h = std::max(1, static_cast<int>(std::lround(static_cast<float>(block.draw_w) * ih / iw)));
        } else {
          block.draw_h = std::max(line_h * 4, static_cast<int>(image_width * 0.62f));
        }
        y += block.draw_h + line_h / 2;
        block.h = block.draw_h + line_h / 2;
      } else {
        const int chars_per_line = std::max(4, width / std::max(8, FontPt()));
        std::string prefix = block.type == FlowBlockType::ListItem ? "- " : "";
        block.lines = WrapUtf8Text(prefix + block.text, chars_per_line);
        const int lines = std::max(1, static_cast<int>(block.lines.size()));
        const int gap = (block.type == FlowBlockType::Header) ? line_h : line_h / 2;
        block.h = lines * line_h + gap;
        y += block.h;
      }
    }
    doc_h = std::max(ViewHeight(), y + kMargin);
    layout_width = width;
    layout_image_width = image_width;
    layout_font_pt = FontPt();
  }

#ifdef HAVE_SDL2_TTF
  TTF_Font *FontForBlock(const FlowBlock &block) const {
    return block.type == FlowBlockType::Header && header_font ? header_font : font;
  }
#endif

  void DrawTextLine(SDL_Renderer *r, const std::string &text, int x, int y, const FlowBlock &block) const {
#ifdef HAVE_SDL2_TTF
    TTF_Font *draw_font = FontForBlock(block);
    if (!draw_font || text.empty()) return;
    const char role = block.type == FlowBlockType::Header ? 'h' : 'b';
    const std::string key = std::string(1, role) + "|" + std::to_string(FontPt()) + "|" +
                            std::to_string(font_color.r) + "," + std::to_string(font_color.g) + "," +
                            std::to_string(font_color.b) + "," + text;
    auto cached = text_textures.find(key);
    if (cached == text_textures.end()) {
      SDL_Surface *surface = TTF_RenderUTF8_Blended(draw_font, text.c_str(), font_color);
      if (!surface) return;
      TextTextureEntry entry;
      entry.texture = SDL_CreateTextureFromSurface(r, surface);
      entry.w = surface->w;
      entry.h = surface->h;
      entry.last_use = SDL_GetTicks();
      SDL_FreeSurface(surface);
      if (!entry.texture) return;
      cached = text_textures.emplace(key, entry).first;
      PruneTextTextures();
    }
    cached->second.last_use = SDL_GetTicks();
    SDL_Rect dst{x, y, cached->second.w, cached->second.h};
    SDL_RenderCopy(r, cached->second.texture, nullptr, &dst);
#else
    (void)r;
    (void)text;
    (void)x;
    (void)y;
    (void)block;
#endif
  }

  void DrawTextBlock(SDL_Renderer *r, const FlowBlock &block, int y) const {
    const int line_h = std::max(16, static_cast<int>(std::lround(FontPt() * 1.45f)));
    int line_y = y;
    for (const auto &line : block.lines) {
      DrawTextLine(r, line, kMargin, line_y, block);
      line_y += line_h;
    }
  }

  bool CreateImageTextureFromResult(SDL_Renderer *r, const AsyncImageRenderResult &ready) const {
    if (!r || !ready.success || ready.job.source_key.empty() || ready.width <= 0 || ready.height <= 0 ||
        ready.rgba.empty()) {
      return false;
    }
    const Uint32 perf_begin = FlowImagePerfLogEnabled() ? SDL_GetTicks() : 0;
    const std::string texture_key =
        FlowImageTextureKey(ready.job.source_key, ready.job.viewport_w, ready.job.viewport_h);
    SDL_Texture *texture = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC,
                                             ready.width, ready.height);
    if (!texture) return false;
    const Uint32 perf_create = FlowImagePerfLogEnabled() ? SDL_GetTicks() : 0;
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    ApplyImageTextureFiltering(texture);
    if (SDL_UpdateTexture(texture, nullptr, ready.rgba.data(), ready.width * 4) != 0) {
      SDL_DestroyTexture(texture);
      return false;
    }
    const Uint32 perf_update = FlowImagePerfLogEnabled() ? SDL_GetTicks() : 0;
    auto old = image_textures.find(texture_key);
    if (old != image_textures.end() && old->second) SDL_DestroyTexture(old->second);
    image_textures[texture_key] = texture;
    image_texture_sizes[texture_key] = SDL_Point{ready.width, ready.height};
    ++image_texture_creates_this_frame;
    if (FlowImagePerfLogEnabled()) {
      runtime_log::Line("[epub_flow_perf] texture resource=" + ready.job.source_key +
                        " size=" + std::to_string(ready.width) + "x" + std::to_string(ready.height) +
                        " create_ms=" + std::to_string(perf_create - perf_begin) +
                        " update_ms=" + std::to_string(perf_update - perf_create));
    }
    return true;
  }

  void DrainReadyImages(SDL_Renderer *r) const {
    AsyncImageRenderResult ready;
    while (image_queue.TakeReady(ready)) {
      const std::string texture_key =
          FlowImageTextureKey(ready.job.source_key, ready.job.viewport_w, ready.job.viewport_h);
      image_pending.erase(texture_key);
      if (!ready.success) {
        ready = AsyncImageRenderResult{};
        continue;
      }
      image_ready[texture_key] = std::move(ready);
      ready = AsyncImageRenderResult{};
    }

    if (!r || image_texture_creates_this_frame >= kMaxImageTextureCreatesPerFrame) return;
    for (auto it = image_ready.begin(); it != image_ready.end();) {
      if (image_texture_creates_this_frame >= kMaxImageTextureCreatesPerFrame) break;
      if (CreateImageTextureFromResult(r, it->second)) {
        it = image_ready.erase(it);
      } else {
        ++it;
      }
    }
  }

  void QueueImageDecode(const FlowBlock &block) const {
    const SDL_Point target = ClampFlowImageTarget(block.image_w, block.image_h, block.draw_w, block.draw_h);
    const std::string texture_key = FlowImageTextureKey(block.resource, target.x, target.y);
    if (block.resource.empty() ||
        image_textures.find(texture_key) != image_textures.end() ||
        image_ready.find(texture_key) != image_ready.end() ||
        image_pending.find(texture_key) != image_pending.end() ||
        image_decode_requests_this_frame > 0 ||
        image_queue.IsBusyOrReady() ||
        !image_ready.empty()) {
      return;
    }
    AsyncImageRenderJob job;
    job.source_key = block.resource;
    job.viewport_w = target.x;
    job.viewport_h = target.y;
    if (image_queue.Request(job, false)) {
      image_pending.insert(texture_key);
      ++image_decode_requests_this_frame;
    }
  }

  SDL_Texture *FindFallbackImageTexture(const std::string &resource, int &w, int &h) const {
    if (resource.empty()) return nullptr;
    const std::string prefix = resource + "|";
    for (const auto &kv : image_textures) {
      if (kv.first.rfind(prefix, 0) != 0 || !kv.second) continue;
      const auto size_it = image_texture_sizes.find(kv.first);
      if (size_it != image_texture_sizes.end()) {
        w = size_it->second.x;
        h = size_it->second.y;
      }
      return kv.second;
    }
    return nullptr;
  }

  SDL_Texture *LoadImage(SDL_Renderer *r, const FlowBlock &block, int &w, int &h) const {
    DrainReadyImages(r);
    const SDL_Point target = ClampFlowImageTarget(block.image_w, block.image_h, block.draw_w, block.draw_h);
    const std::string texture_key = FlowImageTextureKey(block.resource, target.x, target.y);
    const auto tex_size_it = image_texture_sizes.find(texture_key);
    if (tex_size_it != image_texture_sizes.end()) {
      w = tex_size_it->second.x;
      h = tex_size_it->second.y;
    } else {
      w = target.x;
      h = target.y;
    }
    const auto tex_it = image_textures.find(texture_key);
    if (tex_it != image_textures.end()) return tex_it->second;
    SDL_Texture *fallback = FindFallbackImageTexture(block.resource, w, h);
    if (fallback) {
      QueueImageDecode(block);
      return fallback;
    }
    if (image_texture_creates_this_frame >= kMaxImageTextureCreatesPerFrame) return nullptr;
    QueueImageDecode(block);
    return nullptr;
  }

  void QueueImagesAheadOfViewport() const {
    const int prefetch_top = scroll_y + ViewHeight();
    const int prefetch_bottom = prefetch_top + ViewHeight() * 2;
    for (const FlowBlock &block : source_blocks) {
      if (block.y > prefetch_bottom) break;
      if (block.y + block.h < prefetch_top) continue;
      if (block.type != FlowBlockType::Image) continue;
      QueueImageDecode(block);
      if (image_decode_requests_this_frame > 0 || image_queue.IsBusyOrReady() || !image_ready.empty()) return;
    }
  }

  bool LoadMoreDocs(size_t max_docs) {
#ifdef HAVE_LIBZIP
    if (all_docs_loaded || max_docs == 0) return false;
    int zerr = 0;
    zip_t *za = zip_open(path.c_str(), ZIP_RDONLY, &zerr);
    if (!za) return false;
    const size_t before_blocks = source_blocks.size();
    size_t loaded = 0;
    while (next_doc_index < ordered_docs.size() && loaded < max_docs) {
      const size_t doc_index = next_doc_index;
      const std::string doc = ordered_docs[next_doc_index++];
      std::string html;
      if (ReadZipEntry(za, doc, html)) {
        const size_t doc_block_start = source_blocks.size();
        ParseHtmlBlocks(html, doc, source_blocks);
        if (source_blocks.size() > doc_block_start) AddDocAnchor(doc_index, doc, html, doc_block_start);
        ++loaded;
      }
    }
    zip_close(za);
    all_docs_loaded = next_doc_index >= ordered_docs.size();
    if (source_blocks.size() == before_blocks) return false;
    const int start_y = before_blocks > 0
                            ? source_blocks[before_blocks - 1].y + source_blocks[before_blocks - 1].h
                            : kMargin;
    LayoutBlockRange(before_blocks, start_y);
    runtime_log::Line("[epub_flow] lazy loaded docs=" + std::to_string(loaded) +
                      " blocks=" + std::to_string(source_blocks.size()) +
                      " next_doc=" + std::to_string(next_doc_index) + "/" +
                      std::to_string(ordered_docs.size()));
    return true;
#else
    (void)max_docs;
    return false;
#endif
  }

  void LoadAheadIfNeeded() {
    if (all_docs_loaded) return;
    const int threshold = ViewHeight() * kLazyLoadAheadScreens;
    if (doc_h - (scroll_y + ViewHeight()) <= threshold) {
      lazy_load_requested = true;
    }
  }

  size_t FirstVisibleBlockIndex() const {
    size_t lo = 0;
    size_t hi = source_blocks.size();
    const int top = scroll_y;
    while (lo < hi) {
      const size_t mid = lo + (hi - lo) / 2;
      const FlowBlock &block = source_blocks[mid];
      if (block.y + block.h < top) lo = mid + 1;
      else hi = mid;
    }
    return lo;
  }

  void InitializeDocAnchors() {
    doc_anchors.clear();
    doc_anchors.reserve(ordered_docs.size());
    for (size_t i = 0; i < ordered_docs.size(); ++i) {
      FlowDocAnchor anchor;
      anchor.doc_path = ordered_docs[i];
      const auto toc_it = toc_titles_by_doc.find(anchor.doc_path);
      anchor.title = toc_it != toc_titles_by_doc.end() ? toc_it->second
                                                       : u8"\u7b2c " + std::to_string(i + 1) + u8" \u7ae0";
      anchor.doc_index = i;
      doc_anchors.push_back(std::move(anchor));
    }
  }

  void AddDocAnchor(size_t doc_index, const std::string &doc, const std::string &html, size_t block_index) {
    if (doc_index < doc_anchors.size()) {
      doc_anchors[doc_index].doc_path = doc;
      if (toc_titles_by_doc.find(doc) == toc_titles_by_doc.end()) {
        doc_anchors[doc_index].title = TitleFromHtml(html, doc_anchors[doc_index].title);
      }
      doc_anchors[doc_index].block_index = block_index;
      return;
    }
    FlowDocAnchor anchor;
    anchor.doc_path = doc;
    const auto toc_it = toc_titles_by_doc.find(doc);
    anchor.title = toc_it != toc_titles_by_doc.end()
                       ? toc_it->second
                       : TitleFromHtml(html, u8"\u7b2c " + std::to_string(doc_index + 1) + u8" \u7ae0");
    anchor.doc_index = doc_index;
    anchor.block_index = block_index;
    doc_anchors.push_back(std::move(anchor));
  }
};

EpubFlowReader::EpubFlowReader() : impl_(new Impl()) {}

EpubFlowReader::~EpubFlowReader() {
  Close();
  delete impl_;
  impl_ = nullptr;
}

bool EpubFlowReader::Open(const std::string &path, SDL_Renderer *renderer, int screen_w, int screen_h,
                          const EpubRuntimeProgress &initial_progress, int base_font_pt,
                          SDL_Color background_color, SDL_Color font_color) {
  Close();
#if !defined(HAVE_LIBZIP) || !defined(HAVE_SDL2_TTF)
  (void)path;
  (void)renderer;
  (void)screen_w;
  (void)screen_h;
  (void)initial_progress;
  (void)base_font_pt;
  (void)background_color;
  (void)font_color;
  return false;
#else
  int zerr = 0;
  zip_t *za = zip_open(path.c_str(), ZIP_RDONLY, &zerr);
  if (!za) return false;
  ParsedPackage pkg;
  std::string error;
  if (!ParsePackage(za, pkg, error)) {
    zip_close(za);
    runtime_log::Line("[epub_flow] package parse failed path=" + path + " error=" + error);
    return false;
  }
  impl_->path = path;
  impl_->StartImageQueue();
  impl_->ordered_docs = pkg.ordered_docs;
  impl_->toc_titles_by_doc = std::move(pkg.toc_titles_by_doc);
  impl_->next_doc_index = 0;
  impl_->all_docs_loaded = impl_->ordered_docs.empty();
  impl_->InitializeDocAnchors();
  std::vector<FlowBlock> blocks;
  size_t docs_loaded = 0;
  while (impl_->next_doc_index < impl_->ordered_docs.size() && docs_loaded < kInitialFlowDocsToLoad) {
    const size_t doc_index = impl_->next_doc_index;
    const auto &doc = impl_->ordered_docs[impl_->next_doc_index++];
    std::string html;
    if (!ReadZipEntry(za, doc, html)) continue;
    const size_t doc_block_start = blocks.size();
    ParseHtmlBlocks(html, doc, blocks);
    if (blocks.size() > doc_block_start) impl_->AddDocAnchor(doc_index, doc, html, doc_block_start);
    ++docs_loaded;
    if (blocks.size() >= kInitialFlowMinBlocks) break;
  }
  impl_->all_docs_loaded = impl_->next_doc_index >= impl_->ordered_docs.size();
  zip_close(za);
  if (blocks.empty()) {
    runtime_log::Line("[epub_flow] open failed: no flow blocks path=" + path);
    impl_->path.clear();
    impl_->ordered_docs.clear();
    return false;
  }
  impl_->renderer = renderer;
  impl_->screen_w = std::max(1, screen_w);
  impl_->screen_h = std::max(1, screen_h);
  impl_->base_font_pt = std::max(8, base_font_pt);
  impl_->background_color = background_color;
  impl_->font_color = font_color;
  impl_->rotation = NormalizeRotation(initial_progress.rotation);
  impl_->zoom = std::clamp(initial_progress.zoom, kMinZoom, kMaxZoom);
  impl_->source_blocks = std::move(blocks);
  impl_->Relayout();
  const int resume_scroll = std::max(initial_progress.scroll_y,
                                    std::max(0, initial_progress.page) * impl_->ViewHeight());
  while (!impl_->all_docs_loaded && resume_scroll > impl_->MaxScroll() - impl_->ViewHeight()) {
    if (!impl_->LoadMoreDocs(kMoreFlowDocsToLoad)) break;
  }
  impl_->scroll_y = std::clamp(std::max(0, resume_scroll), 0, impl_->MaxScroll());
  runtime_log::Line("[epub_flow] open ok blocks=" + std::to_string(impl_->source_blocks.size()) +
                    " doc_h=" + std::to_string(impl_->doc_h) +
                    " docs=" + std::to_string(docs_loaded) + "/" +
                    std::to_string(impl_->ordered_docs.size()));
  return true;
#endif
}

void EpubFlowReader::Close() {
  if (!impl_) return;
  impl_->image_queue.Shutdown();
  impl_->DestroyImages(true);
  impl_->DestroyTextTextures();
  impl_->CloseFonts();
  impl_->path.clear();
  impl_->ordered_docs.clear();
  impl_->toc_titles_by_doc.clear();
  impl_->next_doc_index = 0;
  impl_->all_docs_loaded = true;
  impl_->lazy_load_requested = false;
  impl_->source_blocks.clear();
  impl_->doc_anchors.clear();
  impl_->scroll_y = 0;
  impl_->doc_h = 1;
  impl_->base_font_pt = kDefaultBaseFontPt;
  impl_->background_color = SDL_Color{250, 249, 244, 255};
  impl_->font_color = SDL_Color{28, 28, 28, 255};
}

bool EpubFlowReader::IsOpen() const { return impl_ && !impl_->path.empty(); }
bool EpubFlowReader::HasRealRenderer() const { return true; }
const char *EpubFlowReader::BackendName() const { return "epub-flow"; }
bool EpubFlowReader::IsRenderPending() const {
  return impl_ && (impl_->image_queue.IsBusyOrReady() || !impl_->image_pending.empty() ||
                   !impl_->image_ready.empty());
}

void EpubFlowReader::UpdateViewport(int screen_w, int screen_h) {
  if (!IsOpen()) return;
  screen_w = std::max(1, screen_w);
  screen_h = std::max(1, screen_h);
  if (impl_->screen_w == screen_w && impl_->screen_h == screen_h) return;
  const float pct = impl_->MaxScroll() > 0 ? static_cast<float>(impl_->scroll_y) / impl_->MaxScroll() : 0.0f;
  impl_->screen_w = screen_w;
  impl_->screen_h = screen_h;
  impl_->scroll_y = 0;
  impl_->Relayout();
  impl_->scroll_y = std::clamp(static_cast<int>(std::lround(pct * impl_->MaxScroll())), 0, impl_->MaxScroll());
}

void EpubFlowReader::Tick() {
  if (!IsOpen()) return;
  if (impl_->lazy_load_requested || !impl_->all_docs_loaded) {
    impl_->lazy_load_requested = false;
    impl_->LoadMoreDocs(kMoreFlowDocsToLoad);
  }
  impl_->QueueImagesAheadOfViewport();
}

void EpubFlowReader::Draw(SDL_Renderer *renderer) const {
  if (!IsOpen() || !renderer) return;
  impl_->image_texture_creates_this_frame = 0;
  impl_->image_decode_requests_this_frame = 0;
  SDL_SetRenderDrawColor(renderer, impl_->background_color.r, impl_->background_color.g,
                         impl_->background_color.b, impl_->background_color.a);
  SDL_Rect bg{0, 0, impl_->screen_w, impl_->screen_h};
  SDL_RenderFillRect(renderer, &bg);
  const int view_h = impl_->ViewHeight();
  SDL_Rect clip{0, 0, impl_->screen_w, impl_->screen_h};
  SDL_RenderSetClipRect(renderer, &clip);
  for (size_t i = impl_->FirstVisibleBlockIndex(); i < impl_->source_blocks.size(); ++i) {
    const auto &block = impl_->source_blocks[i];
    const int y = block.y - impl_->scroll_y;
    if (y > view_h) break;
    if (y + block.h < 0) continue;
    if (block.type == FlowBlockType::Image) {
      int iw = 0;
      int ih = 0;
      SDL_Texture *texture = impl_->LoadImage(renderer, block, iw, ih);
      SDL_Rect dst{std::max(0, (impl_->screen_w - block.draw_w) / 2), y, block.draw_w, block.draw_h};
      if (texture && iw > 0 && ih > 0) {
        const float scale = std::min(static_cast<float>(block.draw_w) / iw,
                                     static_cast<float>(block.draw_h) / ih);
        dst.w = std::max(1, static_cast<int>(iw * scale));
        dst.h = std::max(1, static_cast<int>(ih * scale));
        dst.x = kMargin + (block.draw_w - dst.w) / 2;
        SDL_RenderCopy(renderer, texture, nullptr, &dst);
      } else {
        SDL_SetRenderDrawColor(renderer, 230, 226, 218, 255);
        SDL_RenderFillRect(renderer, &dst);
      }
    } else {
      impl_->DrawTextBlock(renderer, block, y);
    }
  }
  SDL_RenderSetClipRect(renderer, nullptr);
}

void EpubFlowReader::RotateLeft() {
  if (!IsOpen()) return;
  const float pct = impl_->MaxScroll() > 0 ? static_cast<float>(impl_->scroll_y) / impl_->MaxScroll() : 0.0f;
  impl_->rotation = NormalizeRotation(impl_->rotation + 270);
  impl_->Relayout();
  impl_->scroll_y = std::clamp(static_cast<int>(std::lround(pct * impl_->MaxScroll())), 0, impl_->MaxScroll());
}

void EpubFlowReader::RotateRight() {
  if (!IsOpen()) return;
  const float pct = impl_->MaxScroll() > 0 ? static_cast<float>(impl_->scroll_y) / impl_->MaxScroll() : 0.0f;
  impl_->rotation = NormalizeRotation(impl_->rotation + 90);
  impl_->Relayout();
  impl_->scroll_y = std::clamp(static_cast<int>(std::lround(pct * impl_->MaxScroll())), 0, impl_->MaxScroll());
}

void EpubFlowReader::ZoomOut() {
  if (!IsOpen()) return;
  const float pct = impl_->MaxScroll() > 0 ? static_cast<float>(impl_->scroll_y) / impl_->MaxScroll() : 0.0f;
  impl_->zoom = std::max(kMinZoom, impl_->zoom - kZoomStep);
  impl_->Relayout();
  impl_->scroll_y = std::clamp(static_cast<int>(std::lround(pct * impl_->MaxScroll())), 0, impl_->MaxScroll());
}

void EpubFlowReader::ZoomIn() {
  if (!IsOpen()) return;
  const float pct = impl_->MaxScroll() > 0 ? static_cast<float>(impl_->scroll_y) / impl_->MaxScroll() : 0.0f;
  impl_->zoom = std::min(kMaxZoom, impl_->zoom + kZoomStep);
  impl_->Relayout();
  impl_->scroll_y = std::clamp(static_cast<int>(std::lround(pct * impl_->MaxScroll())), 0, impl_->MaxScroll());
}

void EpubFlowReader::ResetView() {
  if (!IsOpen()) return;
  impl_->zoom = 1.0f;
  impl_->Relayout();
}

void EpubFlowReader::SetBaseFontPointSize(int base_font_pt) {
  if (!impl_) return;
  const int clamped = std::max(8, base_font_pt);
  if (impl_->base_font_pt == clamped) return;
  const float pct = impl_->MaxScroll() > 0 ? static_cast<float>(impl_->scroll_y) / impl_->MaxScroll() : 0.0f;
  impl_->base_font_pt = clamped;
  impl_->Relayout();
  impl_->scroll_y = std::clamp(static_cast<int>(std::lround(pct * impl_->MaxScroll())), 0, impl_->MaxScroll());
}

void EpubFlowReader::SetColors(SDL_Color background_color, SDL_Color font_color) {
  if (!impl_) return;
  const bool font_changed = impl_->font_color.r != font_color.r || impl_->font_color.g != font_color.g ||
                            impl_->font_color.b != font_color.b || impl_->font_color.a != font_color.a;
  impl_->background_color = background_color;
  impl_->font_color = font_color;
  if (font_changed) impl_->DestroyTextTextures();
}

void EpubFlowReader::ScrollByPixels(int delta_px) {
  if (!IsOpen()) return;
  impl_->scroll_y = std::clamp(impl_->scroll_y + delta_px, 0, impl_->MaxScroll());
  impl_->LoadAheadIfNeeded();
}

void EpubFlowReader::JumpByScreen(int direction) {
  if (!IsOpen() || direction == 0) return;
  ScrollByPixels(direction * std::max(1, impl_->ViewHeight() - 24));
}

void EpubFlowReader::SetPage(int page_index) {
  if (!IsOpen()) return;
  if (!impl_->all_docs_loaded &&
      page_index >= (impl_->doc_h + impl_->ViewHeight() - 1) / std::max(1, impl_->ViewHeight())) {
    if (impl_->LoadMoreDocs(kMoreFlowDocsToLoad)) {
      impl_->lazy_load_requested = true;
    }
  }
  const int pages = PageCount();
  const int page = std::clamp(page_index, 0, std::max(0, pages - 1));
  impl_->scroll_y = std::clamp(page * impl_->ViewHeight(), 0, impl_->MaxScroll());
  impl_->LoadAheadIfNeeded();
}

std::vector<ReaderChapterAnchor> EpubFlowReader::Chapters() const {
  if (!IsOpen()) return {};
  std::vector<ReaderChapterAnchor> out;
  for (const FlowDocAnchor &doc : impl_->doc_anchors) {
    if (doc.block_index >= impl_->source_blocks.size()) continue;
    ReaderChapterAnchor anchor;
    anchor.title = doc.title.empty() ? u8"\u6b63\u6587" : doc.title;
    anchor.scroll_y = impl_->source_blocks[doc.block_index].y;
    anchor.page = static_cast<int>(doc.doc_index);
    out.push_back(std::move(anchor));
  }
  if (out.size() <= 1 && impl_->doc_h <= impl_->ViewHeight() * 2) return {MakeBodyChapterAnchor()};
  if (out.empty()) out.push_back(MakeBodyChapterAnchor());
  return out;
}

bool EpubFlowReader::ChaptersLoading() const {
  return IsOpen() && impl_ && !impl_->all_docs_loaded;
}

int EpubFlowReader::ChaptersLoadingPercent() const {
  if (!IsOpen() || !impl_) return 0;
  if (impl_->all_docs_loaded) return 100;
  const size_t total = impl_->ordered_docs.size();
  if (total == 0) return 100;
  return std::clamp(static_cast<int>((impl_->next_doc_index * 100) / total), 0, 99);
}

void EpubFlowReader::JumpToChapter(const ReaderChapterAnchor &chapter) {
  if (!IsOpen()) return;
  const size_t selected_index = static_cast<size_t>(std::max(0, chapter.page));
  if (selected_index < impl_->doc_anchors.size()) {
    if (selected_index < impl_->doc_anchors.size()) {
      const size_t block_index = impl_->doc_anchors[selected_index].block_index;
      if (block_index < impl_->source_blocks.size()) {
        impl_->scroll_y = std::clamp(impl_->source_blocks[block_index].y, 0, impl_->MaxScroll());
        impl_->LoadAheadIfNeeded();
        return;
      }
    }
  }
  while (!impl_->all_docs_loaded && chapter.scroll_y > impl_->MaxScroll() - impl_->ViewHeight()) {
    if (!impl_->LoadMoreDocs(kMoreFlowDocsToLoad)) break;
  }
  impl_->scroll_y = std::clamp(std::max(0, chapter.scroll_y), 0, impl_->MaxScroll());
  impl_->LoadAheadIfNeeded();
}

int EpubFlowReader::PageCount() const {
  if (!IsOpen()) return 0;
  const int loaded_pages = std::max(1, (impl_->doc_h + impl_->ViewHeight() - 1) / impl_->ViewHeight());
  if (impl_->all_docs_loaded || impl_->next_doc_index == 0) return loaded_pages;
  const float docs_ratio = static_cast<float>(impl_->ordered_docs.size()) /
                           static_cast<float>(std::max<size_t>(1, impl_->next_doc_index));
  return std::max(loaded_pages, static_cast<int>(std::ceil(loaded_pages * docs_ratio)));
}

bool EpubFlowReader::PageSize(int page_index, int &w, int &h) const {
  (void)page_index;
  if (!IsOpen()) return false;
  w = (impl_->rotation == 90 || impl_->rotation == 270) ? impl_->screen_h : impl_->screen_w;
  h = impl_->ViewHeight();
  return true;
}

int EpubFlowReader::CurrentPage() const {
  if (!IsOpen()) return 0;
  return std::clamp(impl_->scroll_y / std::max(1, impl_->ViewHeight()), 0, std::max(0, PageCount() - 1));
}

EpubRuntimeProgress EpubFlowReader::Progress() const {
  EpubRuntimeProgress out;
  if (!impl_) return out;
  out.page = CurrentPage();
  out.rotation = impl_->rotation;
  out.zoom = impl_->zoom;
  out.scroll_y = impl_->scroll_y;
  return out;
}

bool EpubFlowReader::LooksLikeMixedLayout(const std::string &path) {
#ifndef HAVE_LIBZIP
  (void)path;
  return false;
#else
  int zerr = 0;
  zip_t *za = zip_open(path.c_str(), ZIP_RDONLY, &zerr);
  if (!za) return false;
  ParsedPackage pkg;
  std::string error;
  if (!ParsePackage(za, pkg, error)) {
    zip_close(za);
    runtime_log::Line("[epub_flow] probe package parse failed path=" + path + " error=" + error);
    return false;
  }
  size_t text_chars = 0;
  NaturalTextStats natural_stats;
  size_t image_count = 0;
  size_t docs_read = 0;
  for (const auto &doc : pkg.ordered_docs) {
    std::string html;
    if (!ReadZipEntry(za, doc, html)) continue;
    ++docs_read;
    std::string text = StripTagsToText(html);
    text_chars += text.size();
    NaturalTextStats doc_stats = MeasureNaturalText(text);
    natural_stats.natural_chars += doc_stats.natural_chars;
    natural_stats.natural_blocks += doc_stats.natural_blocks;
    natural_stats.long_natural_blocks += doc_stats.long_natural_blocks;
    natural_stats.content_chars += doc_stats.content_chars;
    natural_stats.meaningful_paragraphs += doc_stats.meaningful_paragraphs;
    natural_stats.max_run = std::max(natural_stats.max_run, doc_stats.max_run);
    image_count += CountImgTagsLinear(html);
    const bool enough_long_text = natural_stats.natural_chars >= kNaturalTextThreshold &&
                                  natural_stats.natural_blocks >= kMinNaturalBlocks &&
                                  natural_stats.long_natural_blocks > 0;
    const bool enough_paragraph_text = natural_stats.content_chars >= kContentTextThreshold &&
                                       natural_stats.meaningful_paragraphs >= kMinMeaningfulParagraphs;
    if (enough_long_text || enough_paragraph_text) {
      break;
    }
    if (docs_read >= 32) break;
  }
  zip_close(za);
  const bool enough_long_text = natural_stats.natural_chars >= kNaturalTextThreshold &&
                                natural_stats.natural_blocks >= kMinNaturalBlocks &&
                                natural_stats.long_natural_blocks > 0;
  const bool enough_paragraph_text = natural_stats.content_chars >= kContentTextThreshold &&
                                     natural_stats.meaningful_paragraphs >= kMinMeaningfulParagraphs;
  const bool has_natural_text = enough_long_text || enough_paragraph_text;
  const bool image_heavy_noise = image_count >= kImageHeavyThreshold &&
                                 natural_stats.meaningful_paragraphs < kMinMeaningfulParagraphs &&
                                 natural_stats.content_chars < kContentTextThreshold;
  const bool flow = has_natural_text && !image_heavy_noise;
  runtime_log::Line("[epub_flow] probe path=" + path + " docs=" + std::to_string(docs_read) +
                    " text=" + std::to_string(text_chars) + " img=" + std::to_string(image_count) +
                    " natural=" + std::to_string(natural_stats.natural_chars) +
                    " content=" + std::to_string(natural_stats.content_chars) +
                    " natural_blocks=" + std::to_string(natural_stats.natural_blocks) +
                    " long_blocks=" + std::to_string(natural_stats.long_natural_blocks) +
                    " paragraphs=" + std::to_string(natural_stats.meaningful_paragraphs) +
                    " max_run=" + std::to_string(natural_stats.max_run) +
                    " result=" + (flow ? "flow" : "comic"));
  return flow;
#endif
}

bool EpubFlowReader::ExtractFirstDocumentImage(const std::string &path, std::string &bytes, std::string &error) {
#ifndef HAVE_LIBZIP
  (void)path;
  (void)bytes;
  error = "libzip unavailable";
  return false;
#else
  int zerr = 0;
  zip_t *za = zip_open(path.c_str(), ZIP_RDONLY, &zerr);
  if (!za) {
    error = "zip open failed";
    return false;
  }
  ParsedPackage pkg;
  if (!ParsePackage(za, pkg, error)) {
    zip_close(za);
    return false;
  }
  const std::regex img_tag_re("<img\\b([^>]*)>", std::regex::icase);
  for (const auto &doc : pkg.ordered_docs) {
    std::string html;
    if (!ReadZipEntry(za, doc, html)) continue;
    const std::string doc_base = std::filesystem::path(doc).parent_path().generic_string();
    for (std::sregex_iterator it(html.begin(), html.end(), img_tag_re), end; it != end; ++it) {
      AttrMap attrs = ParseTagAttrs((*it)[1].str());
      const auto src_it = attrs.find("src");
      if (src_it == attrs.end() || src_it->second.empty()) continue;
      const std::string img_entry = ResolveRelative(doc_base, src_it->second);
      if (ReadZipEntry(za, img_entry, bytes) && !bytes.empty()) {
        zip_close(za);
        return true;
      }
    }
  }
  zip_close(za);
  error = "no document image found";
  return false;
#endif
}
