#pragma once

#include "input_manager.h"

#include <SDL.h>

#include <functional>
#include <string>
#include <vector>

struct TextCacheEntry;

enum class KeyCalibrationPhase {
  Ready,
  Capturing,
  Complete,
  Failed,
};

struct KeyCalibrationEntry {
  Button button = Button::A;
  RawInputBinding binding;
  std::vector<RawInputBinding> bindings;
  bool captured = false;
};

struct KeyCalibrationState {
  bool panel_active = false;
  KeyCalibrationPhase phase = KeyCalibrationPhase::Ready;
  int current_index = 0;
  bool waiting_for_release = false;
  int waiting_for_release_frames = 0;
  std::vector<KeyCalibrationEntry> entries;
  std::string last_sample_text;
  std::string status_text;
};

struct KeyCalibrationCallbacks {
  std::function<bool(KeyCalibrationState &)> save_mapping;
  std::function<void()> exit_app;
};

struct KeyCalibrationRenderDeps {
  SDL_Renderer *renderer = nullptr;
  SDL_Rect preview_rect{};
  const KeyCalibrationState &state;
  bool light_theme = false;
  int language_index = 0;
  float scale = 1.0f;
  std::function<void(int, int, int, int, SDL_Color, bool)> draw_rect;
  std::function<TextCacheEntry *(const std::string &, SDL_Color)> get_text_texture;
  std::function<TextCacheEntry *(const std::string &, SDL_Color)> get_title_text_texture;
};

void InitializeKeyCalibrationState(KeyCalibrationState &state);
bool HasCompletedKeyCalibration(const std::string &mapping_path);
std::string CalibratedKeyGuideTitle(int language_index);
bool SaveKeyCalibrationMapping(const std::string &mapping_path,
                               const std::string &device_model_token,
                               InputProfile input_profile,
                               KeyCalibrationState &state);
bool HandleKeyCalibrationInput(const InputManager &input,
                               InputProfile input_profile,
                               KeyCalibrationState &state,
                               const KeyCalibrationCallbacks &callbacks);
void DrawKeyCalibrationPreview(const KeyCalibrationRenderDeps &deps);
