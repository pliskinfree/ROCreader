#include "key_calibration_runtime.h"

#include "app_language.h"
#include "gkd_menu_button_metrics.h"
#include "ui_text_cache.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <sstream>
#include <unordered_set>

namespace {
constexpr std::array<Button, 15> kCalibrationButtons = {{
    Button::Up,
    Button::Down,
    Button::Left,
    Button::Right,
    Button::A,
    Button::B,
    Button::X,
    Button::Y,
    Button::L1,
    Button::R1,
    Button::L2,
    Button::R2,
    Button::Start,
    Button::Select,
    Button::Menu,
}};

int ScalePx(float scale, int value) {
  return std::max(1, static_cast<int>(std::round(static_cast<float>(value) * std::max(0.1f, scale))));
}

bool IsControlButton(Button button) {
  return button == Button::A ||
         button == Button::B ||
         button == Button::Start ||
         button == Button::Select ||
         button == Button::Menu;
}

bool IsDirectionButton(Button button) {
  return button == Button::Up ||
         button == Button::Down ||
         button == Button::Left ||
         button == Button::Right;
}

bool IsStableDirectionLinuxKey(Button button, int code) {
  constexpr int kKeyUp = 103;
  constexpr int kKeyLeft = 105;
  constexpr int kKeyRight = 106;
  constexpr int kKeyDown = 108;
  switch (button) {
  case Button::Up: return code == kKeyUp;
  case Button::Down: return code == kKeyDown;
  case Button::Left: return code == kKeyLeft;
  case Button::Right: return code == kKeyRight;
  default: return false;
  }
}

bool IsGkdDirectionLinuxKey(Button button, int code) {
  constexpr int kBtnDpadUp = 544;
  constexpr int kBtnDpadDown = 545;
  constexpr int kBtnDpadLeft = 546;
  constexpr int kBtnDpadRight = 547;
  switch (button) {
  case Button::Up: return code == kBtnDpadUp;
  case Button::Down: return code == kBtnDpadDown;
  case Button::Left: return code == kBtnDpadLeft;
  case Button::Right: return code == kBtnDpadRight;
  default: return false;
  }
}

bool IsCalibratableBinding(Button button, InputProfile input_profile, const RawInputBinding &binding) {
  if ((binding.source == RawInputSource::GameControllerAxis ||
       binding.source == RawInputSource::JoystickAxis ||
       binding.source == RawInputSource::LinuxAbs) &&
      binding.direction == 0) {
    return false;
  }
  if (binding.source == RawInputSource::LinuxAbs) {
    return input_profile == InputProfile::RGDS || IsDirectionButton(button);
  }
  if (binding.source == RawInputSource::LinuxKey) {
    if (input_profile == InputProfile::GKD350HUltra) {
      return IsGkdDirectionLinuxKey(button, binding.code) ||
             (!IsDirectionButton(button) && binding.pressed);
    }
    return input_profile == InputProfile::RGDS || IsStableDirectionLinuxKey(button, binding.code);
  }
  return binding.source == RawInputSource::GameControllerButton ||
         binding.source == RawInputSource::JoystickButton ||
         binding.source == RawInputSource::GameControllerAxis ||
         binding.source == RawInputSource::JoystickAxis ||
         binding.source == RawInputSource::JoystickHat;
}

int CalibrationBindingPriority(const RawInputBinding &binding) {
  switch (binding.source) {
  case RawInputSource::JoystickButton:
  case RawInputSource::GameControllerButton:
    return 50;
  case RawInputSource::JoystickHat:
    return 40;
  case RawInputSource::JoystickAxis:
  case RawInputSource::GameControllerAxis:
    return 30;
  case RawInputSource::LinuxAbs:
    return 25;
  case RawInputSource::LinuxKey:
    return 20;
  case RawInputSource::Keyboard:
    return 10;
  default:
    return 0;
  }
}

bool IsShoulderButton(Button button) {
  return button == Button::L2 || button == Button::R2;
}

bool UsesDefaultTrimuiShoulderMapping(InputProfile input_profile, Button button) {
  return input_profile == InputProfile::TrimuiBrick && IsShoulderButton(button);
}

bool IsAxisBinding(const RawInputBinding &binding) {
  return binding.source == RawInputSource::GameControllerAxis ||
         binding.source == RawInputSource::JoystickAxis ||
         binding.source == RawInputSource::LinuxAbs;
}

bool IsAxisReleaseOf(const RawInputBinding &sample, const RawInputBinding &pressed_axis) {
  return IsAxisBinding(sample) &&
         sample.source == pressed_axis.source &&
         sample.code == pressed_axis.code &&
         (sample.direction == 0 || sample.direction != pressed_axis.direction);
}

bool IsReleaseOf(const RawInputBinding &sample, const RawInputBinding &pressed_binding) {
  if (IsAxisBinding(pressed_binding)) return IsAxisReleaseOf(sample, pressed_binding);
  return sample.source == pressed_binding.source &&
         sample.code == pressed_binding.code &&
         !sample.pressed;
}

int CalibrationBindingPriority(Button, InputProfile, const RawInputBinding &binding) {
  return CalibrationBindingPriority(binding);
}

bool AddUniqueBinding(std::vector<RawInputBinding> &bindings, const RawInputBinding &binding) {
  const std::string key = RawInputBindingKey(binding);
  if (key.empty()) return false;
  for (const RawInputBinding &existing : bindings) {
    if (RawInputBindingKey(existing) == key) return false;
  }
  bindings.push_back(binding);
  return true;
}

RawInputBinding LastCapturedBinding(const KeyCalibrationState &state) {
  const int index = state.current_index - 1;
  if (index < 0 || index >= static_cast<int>(state.entries.size())) return RawInputBinding{};
  return state.entries[static_cast<size_t>(index)].binding;
}

void CaptureCurrentEntry(KeyCalibrationState &state,
                         const InputManager &input,
                         const KeyCalibrationCallbacks &callbacks,
                         const RawInputBinding &sample,
                         const std::vector<RawInputBinding> &bindings,
                         bool wait_for_release = true) {
  if (state.current_index < 0 || state.current_index >= static_cast<int>(state.entries.size())) return;
  KeyCalibrationEntry &entry = state.entries[static_cast<size_t>(state.current_index)];
  entry.binding = sample;
  entry.bindings = bindings;
  if (entry.bindings.empty()) entry.bindings.push_back(sample);
  entry.captured = true;
  state.last_sample_text = DescribeRawInputBinding(sample);
  ++state.current_index;
  state.waiting_for_release = wait_for_release;
  state.waiting_for_release_frames = 0;
  input.ClearCalibrationSamples();
  if (state.current_index >= static_cast<int>(state.entries.size())) {
    if (callbacks.save_mapping) callbacks.save_mapping(state);
    else state.phase = KeyCalibrationPhase::Failed;
    state.waiting_for_release = false;
    state.waiting_for_release_frames = 0;
  }
}

void SkipCurrentEntry(KeyCalibrationState &state,
                      const InputManager &input,
                      const KeyCalibrationCallbacks &callbacks) {
  if (state.current_index < 0 || state.current_index >= static_cast<int>(state.entries.size())) return;
  KeyCalibrationEntry &entry = state.entries[static_cast<size_t>(state.current_index)];
  entry.binding = RawInputBinding{};
  entry.bindings.clear();
  entry.captured = false;
  ++state.current_index;
  state.waiting_for_release = false;
  state.waiting_for_release_frames = 0;
  input.ClearCalibrationSamples();
  if (state.current_index >= static_cast<int>(state.entries.size())) {
    if (callbacks.save_mapping) callbacks.save_mapping(state);
    else state.phase = KeyCalibrationPhase::Failed;
  }
}

const char *DisplayButtonName(Button button) {
  switch (button) {
  case Button::Up: return "D-Pad Up";
  case Button::Down: return "D-Pad Down";
  case Button::Left: return "D-Pad Left";
  case Button::Right: return "D-Pad Right";
  case Button::VolUp: return "Vol+";
  case Button::VolDown: return "Vol-";
  default: return ButtonName(button);
  }
}

const char *CalibrationText(int language_index, int id) {
  static const char *texts[12][9] = {
      {u8"\u6309\u952e\u6821\u51c6", u8"\u5f00\u59cb\u6821\u51c6", u8"\u8bf7\u6309\u4e0b", u8"\u5df2\u91c7\u96c6", u8"\u4e2a\u6309\u952e", u8"\u91cd\u542f\u4ee5\u542f\u7528", u8"\u6821\u51c6\u5931\u8d25\uff0c\u5c06\u4f7f\u7528 H700 \u901a\u7528\u6620\u5c04", u8"\u6309 A \u91cd\u542f\u7a0b\u5e8f", u8"\u672c\u673a\u578b\u5df2\u6821\u51c6\u6620\u5c04"},
      {u8"\u6309\u9375\u6821\u6e96", u8"\u958b\u59cb\u6821\u6e96", u8"\u8acb\u6309\u4e0b", u8"\u5df2\u63a1\u96c6", u8"\u500b\u6309\u9375", u8"\u91cd\u555f\u4ee5\u555f\u7528", u8"\u6821\u6e96\u5931\u6557\uff0c\u5c07\u4f7f\u7528 H700 \u901a\u7528\u6620\u5c04", u8"\u6309 A \u91cd\u555f\u7a0b\u5f0f", u8"\u672c\u6a5f\u578b\u5df2\u6821\u6e96\u6620\u5c04"},
      {"Key Calibration", "Start Calibration", "Press", "Captured", "keys", "Restart to enable", "Calibration failed; H700 fallback will be used", "Press A to restart", "This device calibrated mapping"},
      {"Calibrar teclas", "Iniciar", "Pulsa", "Capturadas", "teclas", "Reiniciar para activar", "Fallo; se usara H700", "Pulsa A para reiniciar", "Mapa calibrado del dispositivo"},
      {"Calibrage touches", "Demarrer", "Appuyez", "Capture", "touches", "Redemarrer pour activer", "Echec; profil H700 utilise", "Appuyez A pour redemarrer", "Profil calibre de cet appareil"},
      {"Tasten kalibrieren", "Start", "Drucken", "Erfasst", "Tasten", "Neustart zum Aktivieren", "Fehler; H700 wird genutzt", "A zum Neustart", "Kalibrierte Geraetebelegung"},
      {u8"\u30ad\u30fc\u6821\u6b63", u8"\u6821\u6b63\u958b\u59cb", u8"\u62bc\u3059", u8"\u53d6\u5f97\u6e08\u307f", u8"\u30ad\u30fc", u8"\u518d\u8d77\u52d5\u3067\u6709\u52b9", u8"\u5931\u6557\uff1bH700\u3092\u4f7f\u7528", u8"A\u3067\u518d\u8d77\u52d5", u8"\u3053\u306e\u672c\u4f53\u306e\u6821\u6b63\u30de\u30c3\u30d7"},
      {u8"\ud0a4 \ubcf4\uc815", u8"\ubcf4\uc815 \uc2dc\uc791", u8"\ub204\ub974\uc138\uc694", u8"\uc218\uc9d1\ub428", u8"\ud0a4", u8"\uc7ac\uc2dc\uc791\ud558\uc5ec \uc801\uc6a9", u8"\uc2e4\ud328; H700 \uc0ac\uc6a9", u8"A\ub85c \uc7ac\uc2dc\uc791", u8"\uc774 \uae30\uae30 \ubcf4\uc815 \ub9e4\ud551"},
      {u8"\u0645\u0639\u0627\u064a\u0631\u0629 \u0627\u0644\u0623\u0632\u0631\u0627\u0631", u8"\u0627\u0628\u062f\u0623", u8"\u0627\u0636\u063a\u0637", u8"\u062a\u0645", u8"\u0623\u0632\u0631\u0627\u0631", u8"\u0623\u0639\u062f \u0627\u0644\u062a\u0634\u063a\u064a\u0644", u8"\u0641\u0634\u0644\u062a\u061b H700", u8"\u0627\u0636\u063a\u0637 A", u8"\u062a\u062e\u0637\u064a\u0637 \u0645\u0639\u0627\u064a\u0631"},
      {u8"\u041a\u0430\u043b\u0438\u0431\u0440\u043e\u0432\u043a\u0430", u8"\u041d\u0430\u0447\u0430\u0442\u044c", u8"\u041d\u0430\u0436\u043c\u0438\u0442\u0435", u8"\u0421\u043e\u0431\u0440\u0430\u043d\u043e", u8"\u043a\u043b\u0430\u0432.", u8"\u041f\u0435\u0440\u0435\u0437\u0430\u043f\u0443\u0441\u043a", u8"\u0421\u0431\u043e\u0439; H700", u8"A \u0434\u043b\u044f \u043f\u0435\u0440\u0435\u0437\u0430\u043f\u0443\u0441\u043a\u0430", u8"\u041a\u0430\u043b\u0438\u0431\u0440. \u044d\u0442\u043e\u0433\u043e \u0443\u0441\u0442\u0440."},
      {"Calibrar teclas", "Iniciar", "Pressione", "Capturadas", "teclas", "Reiniciar para ativar", "Falha; H700 sera usado", "Pressione A", "Mapa calibrado do aparelho"},
      {u8"Canh ch\u1ec9nh ph\u00edm", u8"B\u1eaft \u0111\u1ea7u", u8"Nh\u1ea5n", u8"\u0110\u00e3 thu", u8"ph\u00edm", u8"Kh\u1edfi \u0111\u1ed9ng l\u1ea1i", u8"L\u1ed7i; d\u00f9ng H700", u8"Nh\u1ea5n A", u8"S\u01a1 \u0111\u1ed3 \u0111\u00e3 canh ch\u1ec9nh"},
  };
  return texts[ClampSystemLanguageIndex(language_index)][std::clamp(id, 0, 8)];
}

TextCacheEntry *GetText(const KeyCalibrationRenderDeps &deps, const std::string &text,
                        SDL_Color color, bool title = false) {
  if (text.empty()) return nullptr;
  return title && deps.get_title_text_texture
             ? deps.get_title_text_texture(text, color)
             : deps.get_text_texture(text, color);
}

void DrawCenteredText(const KeyCalibrationRenderDeps &deps, const std::string &text,
                      int center_x, int y, SDL_Color color, bool title = false) {
  TextCacheEntry *entry = GetText(deps, text, color, title);
  if (!entry || !entry->texture) return;
  SDL_Rect dst{center_x - entry->w / 2, y, entry->w, entry->h};
  SDL_RenderCopy(deps.renderer, entry->texture, nullptr, &dst);
}

int TextHeight(const KeyCalibrationRenderDeps &deps, const std::string &text,
               SDL_Color color, bool title = false) {
  TextCacheEntry *entry = GetText(deps, text, color, title);
  return entry ? entry->h : 0;
}

void DrawButton(const KeyCalibrationRenderDeps &deps, const std::string &text,
                SDL_Rect rect, bool selected) {
  const SDL_Color fill = deps.light_theme ? SDL_Color{225, 233, 241, 248} : SDL_Color{33, 71, 100, 236};
  const SDL_Color active = deps.light_theme ? SDL_Color{204, 226, 240, 255} : SDL_Color{46, 96, 132, 255};
  const SDL_Color border = deps.light_theme ? SDL_Color{86, 117, 146, 255} : SDL_Color{122, 201, 255, 255};
  const SDL_Color text_color{240, 246, 255, 255};
  deps.draw_rect(rect.x, rect.y, rect.w, rect.h, selected ? active : fill, true);
  deps.draw_rect(rect.x, rect.y, rect.w, rect.h, border, false);
  TextCacheEntry *entry = GetText(deps, text, text_color, false);
  if (!entry || !entry->texture) return;
  SDL_Rect dst{rect.x + (rect.w - entry->w) / 2,
               rect.y + (rect.h - entry->h) / 2,
               entry->w,
               entry->h};
  SDL_RenderCopy(deps.renderer, entry->texture, nullptr, &dst);
}

std::string HeaderModelText(const std::string &device_model_token) {
  if (device_model_token.empty()) return "unknown";
  return device_model_token;
}

bool IsCalibrationHeaderLine(const std::string &line) {
  return line.rfind("# ROCreader calibrated keymap", 0) == 0 ||
         line.rfind("# calibration_model=", 0) == 0 ||
         line.rfind("# calibration_version=", 0) == 0;
}

bool IsCalibrationMappingKey(const std::string &line) {
  const size_t eq = line.find('=');
  if (eq == std::string::npos) return false;
  const std::string key = line.substr(0, eq);
  return key.rfind("joy.", 0) == 0 ||
         key.rfind("pad.", 0) == 0 ||
         key.rfind("key.", 0) == 0 ||
         key.rfind("linux_key.", 0) == 0 ||
         key.rfind("joy_axis.", 0) == 0 ||
         key.rfind("pad_axis.", 0) == 0 ||
         key.rfind("linux_abs.", 0) == 0 ||
         key.rfind("joy_hat.", 0) == 0;
}

bool IsSupportedCalibrationMappingKey(const std::string &line) {
  const size_t eq = line.find('=');
  if (eq == std::string::npos) return false;
  const std::string key = line.substr(0, eq);
  return key.rfind("joy.", 0) == 0 ||
         key.rfind("pad.", 0) == 0 ||
         key.rfind("linux_key.", 0) == 0 ||
         key.rfind("joy_axis.", 0) == 0 ||
         key.rfind("pad_axis.", 0) == 0 ||
         key.rfind("linux_abs.", 0) == 0 ||
         key.rfind("joy_hat.", 0) == 0;
}

std::vector<std::string> ReadExistingNonCalibrationLines(const std::string &mapping_path) {
  std::vector<std::string> lines;
  std::ifstream in(mapping_path);
  if (!in) return lines;
  std::string line;
  while (std::getline(in, line)) {
    if (IsCalibrationHeaderLine(line)) continue;
    if (IsCalibrationMappingKey(line)) continue;
    lines.push_back(line);
  }
  while (!lines.empty() && lines.back().empty()) lines.pop_back();
  return lines;
}

void BeginCalibration(KeyCalibrationState &state, const InputManager &input) {
  for (auto &entry : state.entries) {
    entry.captured = false;
    entry.binding = RawInputBinding{};
    entry.bindings.clear();
  }
  state.panel_active = true;
  state.current_index = 0;
  state.waiting_for_release = true;
  state.waiting_for_release_frames = 0;
  state.phase = KeyCalibrationPhase::Capturing;
  state.status_text.clear();
  state.last_sample_text.clear();
  input.ClearCalibrationSamples();
}
}  // namespace

void InitializeKeyCalibrationState(KeyCalibrationState &state) {
  state = KeyCalibrationState{};
  state.entries.reserve(kCalibrationButtons.size());
  for (Button button : kCalibrationButtons) {
    state.entries.push_back(KeyCalibrationEntry{button, RawInputBinding{}, {}, false});
  }
}

bool HasCompletedKeyCalibration(const std::string &mapping_path) {
  std::ifstream in(mapping_path);
  if (!in) return false;
  std::string line;
  bool saw_header = false;
  bool supported_version = false;
  int mapping_count = 0;
  while (std::getline(in, line)) {
    if (line.rfind("# ROCreader calibrated keymap", 0) == 0) saw_header = true;
    if (line.rfind("# calibration_version=2", 0) == 0 ||
        line.rfind("# calibration_version=3", 0) == 0 ||
        line.rfind("# calibration_version=4", 0) == 0 ||
        line.rfind("# calibration_version=5", 0) == 0) {
      supported_version = true;
    }
    if (IsSupportedCalibrationMappingKey(line)) ++mapping_count;
  }
  return saw_header && supported_version && mapping_count >= 8;
}

std::string CalibratedKeyGuideTitle(int language_index) {
  return CalibrationText(language_index, 8);
}

bool SaveKeyCalibrationMapping(const std::string &mapping_path,
                               const std::string &device_model_token,
                               InputProfile input_profile,
                               KeyCalibrationState &state) {
  std::vector<std::pair<std::string, Button>> mappings;
  std::unordered_set<std::string> seen_keys;
  bool duplicate_mapping = false;
  int captured_entries = 0;
  int required_entries = 0;
  for (const auto &entry : state.entries) {
    if (UsesDefaultTrimuiShoulderMapping(input_profile, entry.button)) continue;
    ++required_entries;
    if (!entry.captured) continue;
    const std::vector<RawInputBinding> &bindings =
        entry.bindings.empty() ? std::vector<RawInputBinding>{entry.binding} : entry.bindings;
    std::string selected_key;
    int selected_priority = -1;
    for (const auto &binding : bindings) {
      if (!IsCalibratableBinding(entry.button, input_profile, binding)) continue;
      const std::string key = RawInputBindingKey(binding);
      if (key.empty()) continue;
      const int priority = CalibrationBindingPriority(entry.button, input_profile, binding);
      if (priority > selected_priority) {
        selected_key = key;
        selected_priority = priority;
      }
    }
    if (selected_key.empty()) continue;
    if (!seen_keys.insert(selected_key).second) {
      duplicate_mapping = true;
      continue;
    }
    mappings.emplace_back(selected_key, entry.button);
    ++captured_entries;
  }
  if (duplicate_mapping || captured_entries < required_entries) {
    state.phase = KeyCalibrationPhase::Failed;
    state.status_text = "missing";
    return false;
  }

  std::vector<std::string> preserved = ReadExistingNonCalibrationLines(mapping_path);
  std::ofstream out(mapping_path, std::ios::trunc);
  if (!out) {
    state.phase = KeyCalibrationPhase::Failed;
    state.status_text = "save failed";
    return false;
  }

  for (const std::string &line : preserved) out << line << "\n";
  if (!preserved.empty()) out << "\n";
  out << "# ROCreader calibrated keymap\n";
  out << "# calibration_version=5\n";
  out << "# calibration_model=" << HeaderModelText(device_model_token) << "\n";
  for (const auto &[key, button] : mappings) {
    out << key << "=" << ButtonName(button) << "\n";
  }
  state.phase = KeyCalibrationPhase::Complete;
  state.status_text.clear();
  return true;
}

bool HandleKeyCalibrationInput(const InputManager &input,
                               InputProfile input_profile,
                               KeyCalibrationState &state,
                               const KeyCalibrationCallbacks &callbacks) {
  RawInputBinding sample;
  if (state.phase == KeyCalibrationPhase::Capturing) {
    if (state.waiting_for_release) {
      const RawInputBinding released_binding = LastCapturedBinding(state);
      while (input.TakeCalibrationSample(sample)) {
        if (IsReleaseOf(sample, released_binding)) {
          state.waiting_for_release = false;
          state.waiting_for_release_frames = 0;
          return true;
        }
        if (sample.pressed && state.current_index >= 0 &&
            state.current_index < static_cast<int>(state.entries.size()) &&
            !UsesDefaultTrimuiShoulderMapping(
                input_profile, state.entries[static_cast<size_t>(state.current_index)].button) &&
            IsCalibratableBinding(state.entries[static_cast<size_t>(state.current_index)].button,
                                  input_profile,
                                  sample)) {
          state.waiting_for_release = false;
          state.waiting_for_release_frames = 0;
          std::vector<RawInputBinding> bindings;
          AddUniqueBinding(bindings, sample);
          RawInputBinding extra_sample;
          KeyCalibrationEntry &entry = state.entries[static_cast<size_t>(state.current_index)];
          while (input.TakeCalibrationSample(extra_sample)) {
            if (!extra_sample.pressed) continue;
            if (!IsCalibratableBinding(entry.button, input_profile, extra_sample)) continue;
            AddUniqueBinding(bindings, extra_sample);
          }
          CaptureCurrentEntry(state, input, callbacks, sample, bindings);
          return true;
        }
      }
      if (input.AnyPressed() && state.waiting_for_release_frames < 3) {
        ++state.waiting_for_release_frames;
        return true;
      }
      state.waiting_for_release = false;
      state.waiting_for_release_frames = 0;
      return true;
    }

    while (state.current_index >= 0 &&
           state.current_index < static_cast<int>(state.entries.size()) &&
           UsesDefaultTrimuiShoulderMapping(
               input_profile, state.entries[static_cast<size_t>(state.current_index)].button)) {
      SkipCurrentEntry(state, input, callbacks);
      if (state.phase != KeyCalibrationPhase::Capturing) return true;
    }

    while (input.TakeCalibrationSample(sample)) {
      if (state.current_index < 0 || state.current_index >= static_cast<int>(state.entries.size())) break;
      KeyCalibrationEntry &entry = state.entries[static_cast<size_t>(state.current_index)];
      if (!sample.pressed) continue;
      if (!IsCalibratableBinding(entry.button, input_profile, sample)) continue;
      if (IsControlButton(entry.button) &&
          sample.source == RawInputSource::Keyboard &&
          (sample.code == SDLK_RETURN || sample.code == SDLK_ESCAPE ||
           sample.code == SDLK_BACKSPACE || sample.code == SDLK_TAB ||
           sample.code == SDLK_m)) {
        continue;
      }
      std::vector<RawInputBinding> bindings;
      AddUniqueBinding(bindings, sample);
      RawInputBinding extra_sample;
      while (input.TakeCalibrationSample(extra_sample)) {
        if (!extra_sample.pressed) continue;
        if (!IsCalibratableBinding(entry.button, input_profile, extra_sample)) continue;
        AddUniqueBinding(bindings, extra_sample);
      }
      CaptureCurrentEntry(state, input, callbacks, sample, bindings);
      return true;
    }
    return false;
  }

  if (!state.panel_active) {
    if (input.IsJustPressed(Button::A) || input.IsJustPressed(Button::Right)) {
      BeginCalibration(state, input);
      return true;
    }
    return false;
  }

  if (input.IsJustPressed(Button::B)) {
    state.panel_active = false;
    if (state.phase == KeyCalibrationPhase::Failed) state.phase = KeyCalibrationPhase::Ready;
    return true;
  }
  if (input.IsJustPressed(Button::A) || input.IsJustPressed(Button::Right)) {
    if (state.phase == KeyCalibrationPhase::Ready || state.phase == KeyCalibrationPhase::Failed) {
      BeginCalibration(state, input);
      return true;
    }
    if (state.phase == KeyCalibrationPhase::Complete) {
      if (callbacks.exit_app) callbacks.exit_app();
      return true;
    }
  }
  return false;
}

void DrawKeyCalibrationPreview(const KeyCalibrationRenderDeps &deps) {
  if (!deps.renderer || !deps.get_text_texture) return;

  const SDL_Color title_color{240, 246, 255, 255};
  const SDL_Color body_color{191, 221, 247, 255};
  const SDL_Color muted_color{142, 170, 196, 255};
  const SDL_Color danger_color{255, 184, 164, 255};
  const int center_x = deps.preview_rect.x + deps.preview_rect.w / 2;
  const int center_y = deps.preview_rect.y + deps.preview_rect.h / 2;
  const int button_h = deps.gkd_profile ? gkd_menu::ControlH(deps.scale) : ScalePx(deps.scale, 38);
  const int button_w = deps.gkd_profile ? gkd_menu::WideButtonW(deps.scale) : ScalePx(deps.scale, 154);
  const int title_y = deps.preview_rect.y + ScalePx(deps.scale, 74);

  if (deps.state.phase == KeyCalibrationPhase::Capturing) {
    DrawCenteredText(deps, CalibrationText(deps.language_index, 0), center_x, title_y, title_color, true);
    const int total = static_cast<int>(deps.state.entries.size());
    const int index = std::clamp(deps.state.current_index, 0, std::max(0, total - 1));
    const Button button = deps.state.entries.empty() ? Button::A : deps.state.entries[static_cast<size_t>(index)].button;
    std::ostringstream progress;
    progress << CalibrationText(deps.language_index, 3) << " "
             << index << "/" << total << " "
             << CalibrationText(deps.language_index, 4);
    DrawCenteredText(deps, progress.str(), center_x, center_y - ScalePx(deps.scale, 78), muted_color);
    DrawCenteredText(deps, CalibrationText(deps.language_index, 2), center_x, center_y - ScalePx(deps.scale, 28),
                     body_color);
    DrawCenteredText(deps, DisplayButtonName(button), center_x, center_y + ScalePx(deps.scale, 8),
                     title_color, true);
    if (!deps.state.last_sample_text.empty()) {
      DrawCenteredText(deps, deps.state.last_sample_text, center_x,
                       center_y + ScalePx(deps.scale, 72), muted_color);
    }
    return;
  }

  if (deps.state.phase == KeyCalibrationPhase::Complete) {
    DrawCenteredText(deps, CalibrationText(deps.language_index, 0), center_x, title_y, title_color, true);
    DrawCenteredText(deps, CalibrationText(deps.language_index, 5), center_x,
                     center_y - ScalePx(deps.scale, 48), body_color, true);
    DrawCenteredText(deps, CalibrationText(deps.language_index, 7), center_x,
                     center_y + ScalePx(deps.scale, 8), muted_color);
    SDL_Rect button_rect{center_x - button_w / 2, center_y + ScalePx(deps.scale, 56), button_w, button_h};
    DrawButton(deps, CalibrationText(deps.language_index, 5), button_rect, true);
    return;
  }

  if (deps.state.phase == KeyCalibrationPhase::Failed) {
    DrawCenteredText(deps, CalibrationText(deps.language_index, 0), center_x, title_y, title_color, true);
    DrawCenteredText(deps, CalibrationText(deps.language_index, 6), center_x,
                     center_y - ScalePx(deps.scale, 62), danger_color);
    SDL_Rect button_rect{center_x - button_w / 2, center_y + ScalePx(deps.scale, 8), button_w, button_h};
    DrawButton(deps, CalibrationText(deps.language_index, 1), button_rect, true);
    return;
  }

  SDL_Rect button_rect{center_x - button_w / 2, center_y - button_h / 2, button_w, button_h};
  const std::string title_text = CalibrationText(deps.language_index, 0);
  const int ready_title_h = TextHeight(deps, title_text, title_color, true);
  const int ready_title_gap = ScalePx(deps.scale, 10);
  DrawCenteredText(deps, title_text, center_x,
                   button_rect.y - ready_title_h - ready_title_gap,
                   title_color, true);
  DrawButton(deps, CalibrationText(deps.language_index, 1), button_rect, deps.state.panel_active);
}
