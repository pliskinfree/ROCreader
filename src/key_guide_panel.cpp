#include "key_guide_panel.h"

#include "app_language.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>

namespace {
int ScalePx(float scale, int value) {
  return std::max(1, static_cast<int>(std::round(static_cast<float>(value) * std::max(0.1f, scale))));
}

struct KeyGuideLine {
  const char *button_label;
  AppTextId action_text;
};

constexpr std::array<KeyGuideLine, 9> kKeyGuideLines = {{
    {"D-Pad", AppTextId::KeyGuideActionDpad},
    {"A", AppTextId::KeyGuideActionA},
    {"B", AppTextId::KeyGuideActionB},
    {"X", AppTextId::KeyGuideActionX},
    {"Y", AppTextId::KeyGuideActionY},
    {"L1 / R1", AppTextId::KeyGuideActionShoulders},
    {"L2 / R2", AppTextId::KeyGuideActionTriggers},
    {"Menu / Start / Select", AppTextId::KeyGuideActionMenu},
    {"Vol+ / Vol-", AppTextId::KeyGuideActionVolume},
}};

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

void DrawKeyGuidePanel(SettingsRuntimeRenderDeps &deps, SDL_Rect preview_rect,
                       int language_index, int first_row_y) {
  if (!deps.renderer) return;

  const SDL_Color title_color{240, 246, 255, 255};
  const SDL_Color key_color{191, 221, 247, 255};
  const SDL_Color divider_color{66, 95, 124, 255};

  auto get_text = [&](const std::string &text, SDL_Color color, bool emphasis = false) -> TextCacheEntry * {
    if (emphasis && deps.get_title_text_texture) return deps.get_title_text_texture(text, color);
    if (deps.get_text_texture) return deps.get_text_texture(text, color);
    return nullptr;
  };

  const float scale = deps.layout.ui_scale;
  const int left = preview_rect.x + ScalePx(scale, 22);
  const int divider_y = first_row_y - ScalePx(scale, 12);
  AppTextId profile_text_id = AppTextId::KeyGuideProfileOtherH700;
  if (deps.input_profile == InputProfile::H70034xxSp) {
    profile_text_id = AppTextId::KeyGuideProfile34xxSp;
  } else if (deps.input_profile == InputProfile::H70035xxH) {
    profile_text_id = AppTextId::KeyGuideProfile35xxH;
  } else if (deps.input_profile == InputProfile::TrimuiBrick) {
    profile_text_id = AppTextId::KeyGuideProfileTrimuiBrick;
  }
  const std::string profile_title = LocalizedAppText(language_index, profile_text_id);
  if (TextCacheEntry *profile = get_text(profile_title, title_color, true); profile && profile->texture) {
    SDL_Rect dst{left, divider_y - profile->h - ScalePx(scale, 8), profile->w, profile->h};
    SDL_RenderCopy(deps.renderer, profile->texture, nullptr, &dst);
  }
  deps.draw_rect(preview_rect.x + ScalePx(scale, 10),
                 divider_y,
                 std::max(0, preview_rect.w - ScalePx(scale, 20)),
                 ScalePx(scale, 1),
                 divider_color,
                 true);

  const int max_text_w = std::max(0, (preview_rect.x + preview_rect.w - ScalePx(scale, 20)) - left);
  const int line_gap = ScalePx(scale, 4);
  const int row_gap = ScalePx(scale, 10);
  const int start_y = divider_y + ScalePx(scale, 16);
  int cursor_y = start_y;
  for (size_t i = 0; i < kKeyGuideLines.size(); ++i) {
    const auto &line = kKeyGuideLines[i];
    const std::string line_text =
        std::string(line.button_label) + ": " + LocalizedAppText(language_index, line.action_text);
    const std::vector<std::string> wrapped_lines = WrapTextByWidth(line_text, max_text_w, get_text, key_color);
    for (size_t line_index = 0; line_index < wrapped_lines.size(); ++line_index) {
      if (TextCacheEntry *line_entry = get_text(wrapped_lines[line_index], key_color);
          line_entry && line_entry->texture) {
        SDL_Rect dst{left, cursor_y, line_entry->w, line_entry->h};
        SDL_RenderCopy(deps.renderer, line_entry->texture, nullptr, &dst);
        cursor_y += line_entry->h + line_gap;
      }
    }
    cursor_y += row_gap;
  }
}
