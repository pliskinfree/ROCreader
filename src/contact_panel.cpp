#include "contact_panel.h"

#include "app_language.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace {
int ScalePx(float scale, int value) {
  return std::max(1, static_cast<int>(std::round(static_cast<float>(value) * std::max(0.1f, scale))));
}

int ContactHintDangerTop(const SettingsRuntimeLayout &layout) {
  if (layout.screen_w == 1024 && layout.screen_h == 768) return 58;
  if (layout.screen_w == 640 && layout.screen_h == 480) return 36;
  return layout.top_bar_y + layout.top_bar_h;
}

size_t Utf8CharSize(unsigned char lead) {
  if ((lead & 0x80u) == 0) return 1;
  if ((lead & 0xE0u) == 0xC0u) return 2;
  if ((lead & 0xF0u) == 0xE0u) return 3;
  if ((lead & 0xF8u) == 0xF0u) return 4;
  return 1;
}

std::string TrimAsciiSpaces(std::string text) {
  while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) {
    text.erase(text.begin());
  }
  while (!text.empty() && (text.back() == ' ' || text.back() == '\t')) {
    text.pop_back();
  }
  return text;
}

std::vector<std::string> WrapTextByWidth(
    const std::string &text, int max_width,
    const std::function<TextCacheEntry *(const std::string &, SDL_Color, bool)> &get_text,
    SDL_Color color, bool emphasis = false) {
  std::vector<std::string> lines;
  if (text.empty() || max_width <= 0) {
    if (!text.empty()) lines.push_back(text);
    return lines;
  }

  auto measure = [&](const std::string &value) -> int {
    if (TextCacheEntry *entry = get_text(value, color, emphasis); entry) return entry->w;
    return 0;
  };

  std::string current;
  size_t last_break = std::string::npos;
  for (size_t pos = 0; pos < text.size();) {
    const size_t char_size = std::min(Utf8CharSize(static_cast<unsigned char>(text[pos])), text.size() - pos);
    const std::string glyph = text.substr(pos, char_size);
    const std::string candidate = current + glyph;
    if (!current.empty() && measure(candidate) > max_width) {
      if (last_break != std::string::npos) {
        std::string line = TrimAsciiSpaces(current.substr(0, last_break));
        std::string remain = TrimAsciiSpaces(current.substr(last_break));
        if (!line.empty()) lines.push_back(line);
        current = remain + glyph;
      } else {
        lines.push_back(TrimAsciiSpaces(current));
        current = glyph;
      }
      last_break = std::string::npos;
      for (size_t i = 0; i < current.size(); ++i) {
        if (current[i] == ' ' || current[i] == '/' || current[i] == ';') last_break = i + 1;
      }
    } else {
      current = candidate;
      if (glyph == " " || glyph == "/" || glyph == ";") last_break = current.size();
    }
    pos += char_size;
  }

  current = TrimAsciiSpaces(current);
  if (!current.empty()) lines.push_back(current);
  return lines;
}
}  // namespace

void DrawContactPanel(SettingsRuntimeRenderDeps &deps, SDL_Rect preview_rect, int language_index) {
  if (!deps.renderer || !deps.get_text_texture) return;

  const SDL_Color hint_color{240, 246, 255, 255};
  const std::string hint_text = LocalizedAppText(language_index, AppTextId::ContactRewardHint);
  if (hint_text.empty()) return;

  const float scale = deps.layout.ui_scale;
  const int safe_x = preview_rect.x + ScalePx(scale, 8);
  const int safe_y = std::max(preview_rect.y + ScalePx(scale, 12), ContactHintDangerTop(deps.layout));
  const int safe_w = std::max(0, preview_rect.w - ScalePx(scale, 16));
  const int safe_h = ScalePx(scale, 46);
  if (safe_w <= 0 || safe_h <= 0) return;

  using TextGetter = std::function<TextCacheEntry *(const std::string &, SDL_Color)>;
  std::vector<TextGetter> getters;
  if (deps.get_text_texture) getters.push_back(deps.get_text_texture);
  if (deps.get_reader_text_texture) getters.push_back(deps.get_reader_text_texture);

  std::vector<std::string> fitted_lines;
  int total_h = 0;
  int line_gap = 0;
  int max_line_w = 0;

  auto measure_lines = [&](const TextGetter &getter, int wrap_width) -> bool {
    fitted_lines = WrapTextByWidth(
        hint_text, wrap_width,
        [&](const std::string &text, SDL_Color color, bool) -> TextCacheEntry * { return getter ? getter(text, color) : nullptr; },
        hint_color);
    if (fitted_lines.empty()) return false;

    total_h = 0;
    line_gap = 2;
    max_line_w = 0;
    for (const std::string &line : fitted_lines) {
      TextCacheEntry *entry = getter ? getter(line, hint_color) : nullptr;
      if (!entry || !entry->texture) return false;
      max_line_w = std::max(max_line_w, entry->w);
      total_h += entry->h;
    }
    total_h += std::max(0, static_cast<int>(fitted_lines.size()) - 1) * line_gap;
    return true;
  };

  TextGetter render_getter = nullptr;
  float render_scale = 1.0f;
  for (const TextGetter &getter : getters) {
    if (!measure_lines(getter, safe_w)) continue;
    const float width_scale = (max_line_w > 0) ? (static_cast<float>(safe_w) / static_cast<float>(max_line_w)) : 1.0f;
    const float height_scale = (total_h > 0) ? (static_cast<float>(safe_h) / static_cast<float>(total_h)) : 1.0f;
    const float candidate_scale = std::min(1.0f, std::min(width_scale, height_scale));
    if (candidate_scale > 0.0f) {
      render_getter = getter;
      render_scale = candidate_scale;
      break;
    }
  }
  if (!render_getter) return;

  const int scaled_total_h = std::max(1, static_cast<int>(std::round(total_h * render_scale)));
  const int scaled_gap = std::max(0, static_cast<int>(std::round(line_gap * render_scale)));
  const int block_y = safe_y + std::max(0, (safe_h - scaled_total_h) / 2);
  int cursor_y = block_y;

  for (size_t i = 0; i < fitted_lines.size(); ++i) {
    TextCacheEntry *entry = render_getter(fitted_lines[i], hint_color);
    if (!entry || !entry->texture) continue;
    const int scaled_w = std::max(1, static_cast<int>(std::round(entry->w * render_scale)));
    const int scaled_h = std::max(1, static_cast<int>(std::round(entry->h * render_scale)));
    const int line_x = safe_x + std::max(0, (safe_w - scaled_w) / 2);
    SDL_Rect dst{line_x, cursor_y, scaled_w, scaled_h};
    SDL_RenderCopy(deps.renderer, entry->texture, nullptr, &dst);
    cursor_y += scaled_h + scaled_gap;
  }
}
