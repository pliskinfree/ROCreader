#include "online_source_panel.h"

#include "app_language.h"

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

TextCacheEntry *GetTextEntry(SettingsRuntimeRenderDeps &deps, const std::string &text, SDL_Color color,
                             bool title = false) {
  if (text.empty()) return nullptr;
  return title && deps.services.get_title_text_texture
             ? deps.services.get_title_text_texture(text, color)
             : deps.services.get_text_texture(text, color);
}

std::string TruncateToWidth(SettingsRuntimeRenderDeps &deps, const std::string &text, int max_w, SDL_Color color) {
  if (text.empty() || max_w <= 0) return {};
  if (TextCacheEntry *entry = GetTextEntry(deps, text, color); entry && entry->w <= max_w) return text;
  if (deps.services.utf8_ellipsize) {
    for (size_t chars = 48; chars > 4; --chars) {
      std::string candidate = deps.services.utf8_ellipsize(text, chars);
      if (TextCacheEntry *entry = GetTextEntry(deps, candidate, color); entry && entry->w <= max_w) return candidate;
    }
  }
  std::string candidate = text;
  while (!candidate.empty()) {
    candidate.pop_back();
    std::string ellipsized = candidate + "...";
    if (TextCacheEntry *entry = GetTextEntry(deps, ellipsized, color); entry && entry->w <= max_w) return ellipsized;
  }
  return {};
}

void DrawClippedMarqueeText(SettingsRuntimeRenderDeps &deps, const std::string &text, SDL_Rect clip,
                            SDL_Color color) {
  if (clip.w <= 0 || clip.h <= 0) return;
  TextCacheEntry *entry = GetTextEntry(deps, text, color);
  if (!entry || !entry->texture) return;
  SDL_RenderSetClipRect(deps.renderer, &clip);
  const int text_y = clip.y + std::max(0, (clip.h - entry->h) / 2);
  if (entry->w <= clip.w) {
    SDL_Rect dst{clip.x, text_y, entry->w, entry->h};
    SDL_RenderCopy(deps.renderer, entry->texture, nullptr, &dst);
  } else {
    const int gap = ScalePx(deps.layout.ui_scale, 32);
    const int span = std::max(1, entry->w + gap);
    const int offset = static_cast<int>((SDL_GetTicks() / 28) % static_cast<uint32_t>(span));
    SDL_Rect dst1{clip.x - offset, text_y, entry->w, entry->h};
    SDL_Rect dst2{dst1.x + span, text_y, entry->w, entry->h};
    SDL_RenderCopy(deps.renderer, entry->texture, nullptr, &dst1);
    SDL_RenderCopy(deps.renderer, entry->texture, nullptr, &dst2);
    const int bar_h = ScalePx(deps.layout.ui_scale, 2);
    const int handle_w = std::max(ScalePx(deps.layout.ui_scale, 18),
                                  static_cast<int>(std::round(static_cast<float>(clip.w) * clip.w /
                                                              std::max(1, entry->w))));
    const int travel = std::max(1, clip.w - handle_w);
    const int handle_x = clip.x + (travel * offset) / span;
    deps.services.draw_rect(clip.x, clip.y + clip.h - bar_h, clip.w, bar_h, SDL_Color{55, 75, 94, 150}, true);
    deps.services.draw_rect(handle_x, clip.y + clip.h - bar_h, handle_w, bar_h, color, true);
  }
  SDL_RenderSetClipRect(deps.renderer, nullptr);
}

int ButtonRowStart(const OnlineSourceState &state) {
  return static_cast<int>(state.sources.size());
}

const char *OnlineText(int language_index, int id) {
  static const char *texts[12][14] = {
      {u8"URL\u5165\u53e3", u8"\u8fde\u63a5", u8"\u6e05\u9664\u7f13\u5b58", u8"\u9000\u51fa\u8fde\u63a5", u8"\u672a\u8fde\u63a5", u8"\u8fde\u63a5\u4e2d", u8"\u5df2\u8fde\u63a5", u8"\u65e0\u53ef\u7528\u8fde\u63a5\u6e90", u8"\u5df2\u9009\u62e9", u8"\u5df2\u52a0\u8f7d", u8"\u4e2a\u8fde\u63a5\u6e90", u8"\u76ee\u5f55\u52a0\u8f7d\u5931\u8d25", u8"\u5df2\u65ad\u5f00", u8"\u7f13\u5b58\u5df2\u6e05\u9664"},
      {u8"URL\u5165\u53e3", u8"\u9023\u63a5", u8"\u6e05\u9664\u5feb\u53d6", u8"\u65b7\u958b\u9023\u63a5", u8"\u672a\u9023\u63a5", u8"\u9023\u63a5\u4e2d", u8"\u5df2\u9023\u63a5", u8"\u6c92\u6709\u53ef\u7528\u9023\u63a5\u6e90", u8"\u5df2\u9078\u64c7", u8"\u5df2\u8f09\u5165", u8"\u500b\u9023\u63a5\u6e90", u8"\u76ee\u9304\u8f09\u5165\u5931\u6557", u8"\u5df2\u65b7\u958b", u8"\u5feb\u53d6\u5df2\u6e05\u9664"},
      {"URL Entry", "Connect", "Clear Cache", "Disconnect", "Not connected", "Connecting", "Connected", "No online sources", "Selected", "Loaded", "source(s)", "Catalog load failed", "Disconnected", "Cache cleared"},
      {"Entrada URL", "Conectar", "Borrar cache", "Desconectar", "Sin conexion", "Conectando", "Conectado", "Sin fuentes", "Seleccionado", "Cargado", "fuente(s)", "Fallo catalogo", "Desconectado", "Cache borrada"},
      {"Entree URL", "Connecter", "Vider cache", "Deconnecter", "Non connecte", "Connexion", "Connecte", "Aucune source", "Selectionne", "Charge", "source(s)", "Echec catalogue", "Deconnecte", "Cache vide"},
      {"URL Eingang", "Verbinden", "Cache leeren", "Trennen", "Nicht verbunden", "Verbinde", "Verbunden", "Keine Quellen", "Gewahlt", "Geladen", "Quelle(n)", "Katalogfehler", "Getrennt", "Cache geleert"},
      {u8"URL\u5165\u53e3", u8"\u63a5\u7d9a", u8"\u30ad\u30e3\u30c3\u30b7\u30e5\u6d88\u53bb", u8"\u5207\u65ad", u8"\u672a\u63a5\u7d9a", u8"\u63a5\u7d9a\u4e2d", u8"\u63a5\u7d9a\u6e08\u307f", u8"\u30bd\u30fc\u30b9\u306a\u3057", u8"\u9078\u629e\u6e08\u307f", u8"\u8aad\u8fbc\u6e08\u307f", u8"\u4ef6", u8"\u30ab\u30bf\u30ed\u30b0\u5931\u6557", u8"\u5207\u65ad\u6e08\u307f", u8"\u6d88\u53bb\u6e08\u307f"},
      {u8"URL \uc785\uad6c", u8"\uc5f0\uacb0", u8"\uce90\uc2dc \uc0ad\uc81c", u8"\uc5f0\uacb0 \ub04a\uae30", u8"\ubbf8\uc5f0\uacb0", u8"\uc5f0\uacb0 \uc911", u8"\uc5f0\uacb0\ub428", u8"\uc18c\uc2a4 \uc5c6\uc74c", u8"\uc120\ud0dd\ub428", u8"\ub85c\ub4dc\ub428", u8"\uc18c\uc2a4", u8"\uce74\ud0c8\ub85c\uadf8 \uc2e4\ud328", u8"\uc5f0\uacb0 \ub04a\uae40", u8"\uce90\uc2dc \uc0ad\uc81c\ub428"},
      {u8"\u0645\u062f\u062e\u0644 URL", u8"\u0627\u062a\u0635\u0627\u0644", u8"\u0645\u0633\u062d \u0627\u0644\u0630\u0627\u0643\u0631\u0629", u8"\u0642\u0637\u0639", u8"\u063a\u064a\u0631 \u0645\u062a\u0635\u0644", u8"\u064a\u062a\u0635\u0644", u8"\u0645\u062a\u0635\u0644", u8"\u0644\u0627 \u0645\u0635\u0627\u062f\u0631", u8"\u0645\u062d\u062f\u062f", u8"\u062a\u0645 \u0627\u0644\u062a\u062d\u0645\u064a\u0644", u8"\u0645\u0635\u062f\u0631", u8"\u0641\u0634\u0644 \u0627\u0644\u0641\u0647\u0631\u0633", u8"\u0645\u0641\u0635\u0648\u0644", u8"\u062a\u0645 \u0627\u0644\u0645\u0633\u062d"},
      {u8"URL \u0432\u0445\u043e\u0434", u8"\u041f\u043e\u0434\u043a\u043b.", u8"\u041e\u0447\u0438\u0441\u0442\u0438\u0442\u044c", u8"\u041e\u0442\u043a\u043b.", u8"\u041d\u0435 \u043f\u043e\u0434\u043a\u043b.", u8"\u041f\u043e\u0434\u043a\u043b.", u8"\u041f\u043e\u0434\u043a\u043b.", u8"\u041d\u0435\u0442 \u0438\u0441\u0442\u043e\u0447\u043d.", u8"\u0412\u044b\u0431\u0440\u0430\u043d\u043e", u8"\u0417\u0430\u0433\u0440.", u8"\u0438\u0441\u0442.", u8"\u0421\u0431\u043e\u0439 \u043a\u0430\u0442\u0430\u043b\u043e\u0433\u0430", u8"\u041e\u0442\u043a\u043b.", u8"\u041a\u044d\u0448 \u043e\u0447\u0438\u0449\u0435\u043d"},
      {"Entrada URL", "Conectar", "Limpar cache", "Desconectar", "Sem conexao", "Conectando", "Conectado", "Sem fontes", "Selecionado", "Carregado", "fonte(s)", "Falha catalogo", "Desconectado", "Cache limpo"},
      {u8"M\u1ee5c URL", u8"K\u1ebft n\u1ed1i", u8"X\u00f3a cache", u8"Ng\u1eaft k\u1ebft n\u1ed1i", u8"Ch\u01b0a k\u1ebft n\u1ed1i", u8"\u0110ang k\u1ebft n\u1ed1i", u8"\u0110\u00e3 k\u1ebft n\u1ed1i", u8"Kh\u00f4ng c\u00f3 ngu\u1ed3n", u8"\u0110\u00e3 ch\u1ecdn", u8"\u0110\u00e3 t\u1ea3i", u8"ngu\u1ed3n", u8"L\u1ed7i danh m\u1ee5c", u8"\u0110\u00e3 ng\u1eaft", u8"\u0110\u00e3 x\u00f3a cache"},
  };
  return texts[ClampSystemLanguageIndex(language_index)][std::clamp(id, 0, 13)];
}

std::string LocalizedStatus(const OnlineSourceState &state, int language_index) {
  if (state.connecting || state.connect_pending) return OnlineText(language_index, 5);
  if (state.connected && state.active_source_index >= 0 &&
      state.active_source_index < static_cast<int>(state.sources.size())) {
    return std::string(OnlineText(language_index, 6)) + ": " + state.sources[state.active_source_index].name;
  }
  if (state.sources.empty()) return OnlineText(language_index, 7);
  return OnlineText(language_index, 4);
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
      state.status_message.clear();
      return true;
    }
    if (state.selected_button == 0) {
      state.connect_pending = true;
      state.connecting = true;
      state.status_message.clear();
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
                                 ? deps.services.get_title_text_texture(OnlineText(language_index, 0), text)
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
    const int source_text_x = box_x + box + ScalePx(scale, 10);
    const std::string source_text =
        TruncateToWidth(deps, source.url, std::max(1, rx + rw - source_text_x - ScalePx(scale, 8)),
                        source.enabled ? text : muted);
    DrawText(deps, source_text, source_text_x, y + ScalePx(scale, 6), source.enabled ? text : muted);
    y += row_h + row_gap;
  }
  if (state.sources.empty()) {
    DrawText(deps, OnlineText(language_index, 7),
             left, y, text);
  }

  const char *labels[3] = {OnlineText(language_index, 1), OnlineText(language_index, 2),
                           OnlineText(language_index, 3)};
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

  const int status_y = button_y - ScalePx(scale, 28);
  const int bar_w = (state.connecting || state.connect_pending) ? std::max(ScalePx(scale, 76), (right - left) / 4) : 0;
  const int gap = ScalePx(scale, 10);
  const int status_w = std::max(1, right - left - (bar_w > 0 ? bar_w + gap : 0));
  DrawClippedMarqueeText(deps, LocalizedStatus(state, language_index),
                         SDL_Rect{left, status_y, status_w, ScalePx(scale, 22)}, accent);
  if (state.connecting || state.connect_pending) {
    const int bar_x = left + status_w + gap;
    const int bar_y = status_y + ScalePx(scale, 9);
    const int bar_h = ScalePx(scale, 4);
    const int phase = static_cast<int>((SDL_GetTicks() / 160) % 6);
    deps.services.draw_rect(bar_x, bar_y, bar_w, bar_h, border, false);
    deps.services.draw_rect(bar_x + (bar_w * phase) / 6, bar_y, std::max(ScalePx(scale, 18), bar_w / 4), bar_h,
                            accent, true);
  }
}
