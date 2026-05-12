#include "online_source_panel.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace {
int ScalePx(float scale, int value) {
  return std::max(1, static_cast<int>(std::round(static_cast<float>(value) * std::max(0.1f, scale))));
}

void DrawText(SettingsRuntimeRenderDeps &deps, const std::string &text, int x, int y, SDL_Color color,
              bool title = false) {
  if (text.empty()) return;
  TextCacheEntry *entry = title && deps.services.get_title_text_texture
                              ? deps.services.get_title_text_texture(text, color)
                              : deps.services.get_text_texture(text, color);
  if (!entry || !entry->texture) return;
  SDL_Rect dst{x, y, entry->w, entry->h};
  SDL_RenderCopy(deps.renderer, entry->texture, nullptr, &dst);
}

int ButtonRowStart(const OnlineSourceState &state) {
  return static_cast<int>(state.sources.size());
}
}  // namespace

bool HandleOnlineSourcePanelInput(SettingsRuntimeInputDeps &deps) {
  OnlineSourceState &state = deps.online_source_state;
  const int source_count = static_cast<int>(state.sources.size());
  const int row_count = source_count + 1;
  const int button_row = ButtonRowStart(state);

  if (!state.panel_active) {
    if (deps.input.IsJustPressed(Button::A) || deps.input.IsJustPressed(Button::Right)) {
      state.panel_active = true;
      state.selected_row = std::clamp(state.selected_row, 0, std::max(0, row_count - 1));
      return true;
    }
    return false;
  }
  if (deps.input.IsJustPressed(Button::B)) {
    state.panel_active = false;
    return true;
  }
  if (deps.input.IsJustPressed(Button::X)) {
    ReloadOnlineSourceConfig(state);
    return true;
  }
  if (deps.input.IsJustPressed(Button::Up) || deps.input.IsRepeated(Button::Up)) {
    state.selected_row = (state.selected_row - 1 + row_count) % row_count;
    return true;
  }
  if (deps.input.IsJustPressed(Button::Down) || deps.input.IsRepeated(Button::Down)) {
    state.selected_row = (state.selected_row + 1) % row_count;
    return true;
  }
  if (state.selected_row == button_row) {
    if (deps.input.IsJustPressed(Button::Left) || deps.input.IsRepeated(Button::Left)) {
      state.selected_button = (state.selected_button + 2) % 3;
      return true;
    }
    if (deps.input.IsJustPressed(Button::Right) || deps.input.IsRepeated(Button::Right)) {
      state.selected_button = (state.selected_button + 1) % 3;
      return true;
    }
  }
  if (deps.input.IsJustPressed(Button::A)) {
    if (state.selected_row < source_count) {
      state.selected_source_index = state.selected_row;
      state.connected = false;
      state.active_source_index = -1;
      state.status_message = "Selected: " + state.sources[state.selected_source_index].name;
      return true;
    }
    if (state.selected_button == 0) {
      state.connect_pending = true;
      state.connecting = true;
      state.status_message = "Connecting...";
    }
    else if (state.selected_button == 1) ClearOnlineSourceDownloads(state);
    else state.disconnect_requested = true;
    return true;
  }
  return false;
}

void DrawOnlineSourcePanel(SettingsRuntimeRenderDeps &deps, SDL_Rect preview_rect,
                           int language_index, int first_menu_item_y,
                           int sidebar_item_pitch, int sidebar_item_h, float scale) {
  (void)language_index;
  const OnlineSourceState &state = deps.online_source_state;
  const bool light = deps.cfg.theme != 0;
  const SDL_Color divider_color{66, 95, 124, 255};
  const SDL_Color border = light ? SDL_Color{130, 145, 160, 255} : SDL_Color{73, 111, 146, 255};
  const SDL_Color text = light ? SDL_Color{25, 31, 38, 255} : SDL_Color{232, 239, 248, 255};
  const SDL_Color muted = light ? SDL_Color{92, 103, 115, 255} : SDL_Color{164, 178, 194, 255};
  const SDL_Color accent = SDL_Color{92, 171, 218, 255};

  SDL_Rect safe_rect = preview_rect;
  const int top_safe = deps.layout.top_bar_y + deps.layout.top_bar_h;
  const int bottom_safe = deps.layout.bottom_bar_y;
  if (safe_rect.y < top_safe) {
    const int delta = top_safe - safe_rect.y;
    safe_rect.y += delta;
    safe_rect.h = std::max(1, safe_rect.h - delta);
  }
  if (safe_rect.y + safe_rect.h > bottom_safe) {
    safe_rect.h = std::max(1, bottom_safe - safe_rect.y);
  }

  const int left = preview_rect.x + ScalePx(scale, 22);
  const int right = preview_rect.x + preview_rect.w - ScalePx(scale, 20);
  const int divider_y = first_menu_item_y - ScalePx(scale, 12);
  if (TextCacheEntry *title = deps.services.get_title_text_texture
                                 ? deps.services.get_title_text_texture(u8"URL\u5165\u53e3", text)
                                 : nullptr;
      title && title->texture) {
    SDL_Rect dst{left, divider_y - title->h - ScalePx(scale, 8), title->w, title->h};
    SDL_RenderCopy(deps.renderer, title->texture, nullptr, &dst);
  }
  deps.services.draw_rect(preview_rect.x + ScalePx(scale, 10),
                          divider_y,
                          std::max(0, preview_rect.w - ScalePx(scale, 20)),
                          ScalePx(scale, 1),
                          divider_color,
                          true);
  int y = divider_y + ScalePx(scale, 16);

  const int row_h = ScalePx(scale, 31);
  const int row_gap = ScalePx(scale, 7);
  int exit_row_index = 0;
  for (size_t i = 0; i < deps.menu_items.size(); ++i) {
    if (deps.menu_items[i] == SettingId::ExitApp) {
      exit_row_index = static_cast<int>(i);
      break;
    }
  }
  const int button_y = first_menu_item_y + sidebar_item_pitch * exit_row_index;
  const int button_h = sidebar_item_h;
  const int list_bottom = std::min(button_y - ScalePx(scale, 14), safe_rect.y + safe_rect.h);
  const int max_rows = std::max(1, (list_bottom - y) / std::max(1, row_h + row_gap));
  const int visible_sources = std::min<int>(state.sources.size(), max_rows);
  for (int i = 0; i < visible_sources; ++i) {
    const OnlineSourceEntry &source = state.sources[i];
    const bool row_selected = state.panel_active && state.selected_row == i;
    const bool checked = state.selected_source_index == i;
    const SDL_Color row_color = row_selected ? SDL_Color{54, 103, 139, 220}
                                             : (light ? SDL_Color{229, 233, 238, 220}
                                                      : SDL_Color{42, 54, 70, 216});
    const int rx = left;
    const int rw = std::max(1, right - left);
    deps.services.draw_rect(rx, y, rw, row_h, row_color, true);
    deps.services.draw_rect(rx, y, rw, row_h, row_selected ? accent : border, false);

    const int box = ScalePx(scale, 14);
    const int box_x = rx + ScalePx(scale, 10);
    const int box_y = y + std::max(0, (row_h - box) / 2);
    deps.services.draw_rect(box_x, box_y, box, box, checked ? accent : muted, false);
    if (checked) deps.services.draw_rect(box_x + ScalePx(scale, 3), box_y + ScalePx(scale, 3),
                                         box - ScalePx(scale, 6), box - ScalePx(scale, 6), accent, true);
    DrawText(deps, source.url, box_x + box + ScalePx(scale, 10),
             y + ScalePx(scale, 6), source.enabled ? text : muted);
    y += row_h + row_gap;
  }
  if (state.sources.empty()) {
    DrawText(deps, "No sources. Edit online_sources.ini and press X to reload.",
             left, y, text);
  }

  const char *labels[3] = {u8"\u8fde\u63a5", u8"\u6e05\u9664\u7f13\u5b58", u8"\u9000\u51fa\u8fde\u63a5"};
  const int button_gap = ScalePx(scale, 12);
  const int button_area_w = std::max(1, right - left);
  const int button_w = std::max(ScalePx(scale, 92), (button_area_w - button_gap * 2) / 3);
  for (int i = 0; i < 3; ++i) {
    const int bx = left + i * (button_w + button_gap);
    const bool selected = state.panel_active && state.selected_row == ButtonRowStart(state) && state.selected_button == i;
    deps.services.draw_rect(bx, button_y, button_w, button_h,
                            selected ? SDL_Color{70, 126, 164, 238}
                                     : (light ? SDL_Color{218, 224, 230, 238}
                                              : SDL_Color{45, 61, 78, 238}),
                            true);
    deps.services.draw_rect(bx, button_y, button_w, button_h, selected ? accent : border, false);
    if (TextCacheEntry *label = deps.services.get_text_texture(labels[i], text); label && label->texture) {
      SDL_Rect dst{bx + std::max(0, (button_w - label->w) / 2),
                   button_y + std::max(0, (button_h - label->h) / 2),
                   label->w,
                   label->h};
      SDL_RenderCopy(deps.renderer, label->texture, nullptr, &dst);
    }
  }

  const std::string state_text = state.connecting
                                     ? std::string(u8"\u8fde\u63a5\u4e2d...")
                                     : state.connected && state.active_source_index >= 0 &&
                                         state.active_source_index < static_cast<int>(state.sources.size())
                                     ? std::string(u8"\u5df2\u8fde\u63a5: ") + state.sources[state.active_source_index].name
                                     : std::string(u8"\u672a\u8fde\u63a5");
  DrawText(deps, state_text, left, button_y - ScalePx(scale, 28), accent);
  if (state.connecting) {
    const int bar_x = left;
    const int bar_y = button_y - ScalePx(scale, 10);
    const int bar_w = std::max(ScalePx(scale, 96), (right - left) / 3);
    const int bar_h = ScalePx(scale, 4);
    const int phase = static_cast<int>((SDL_GetTicks() / 160) % 6);
    deps.services.draw_rect(bar_x, bar_y, bar_w, bar_h, border, false);
    deps.services.draw_rect(bar_x + (bar_w * phase) / 6, bar_y, std::max(ScalePx(scale, 18), bar_w / 4), bar_h,
                            accent, true);
  }
  if (!state.status_message.empty()) {
    DrawText(deps, state.status_message, left + ScalePx(scale, 180),
             button_y - ScalePx(scale, 28), muted);
  }
}
