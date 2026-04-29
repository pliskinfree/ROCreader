#pragma once

#include "app_context.h"
#include "animation.h"
#include "input_manager.h"
#include "scene_manager.h"

#include <SDL.h>

#include <cstdint>
#include <functional>

struct AppEventPumpResult {
  bool had_event = false;
  bool timed_out = false;
};

struct AppFrameTiming {
  uint32_t frame_begin_ticks = 0;
  uint32_t now = 0;
  float dt = 0.0f;
};

class AppShell {
public:
  void Initialize(AppContext &context);
  AppContext *Context();
  const AppContext *Context() const;
  SceneManager &Scenes();
  const SceneManager &Scenes() const;

  bool IsRunning() const;
  void RequestQuit();

  AppFrameTiming BeginFrame(uint32_t &previous_ticks) const;
  void ResetFrameClock(uint32_t &previous_ticks) const;
  void BeginDraw() const;
  void Present() const;

  void StartSceneFlash();
  void TickSceneFlash(float dt, bool animate_enabled);
  bool IsSceneFlashAnimating() const;
  void DrawSceneFlash() const;

  AppEventPumpResult PumpEvents(InputManager &input, bool active, int idle_wait_ms,
                                const std::function<void(const SDL_Event &)> &note_user_input);

  void ThrottleFrame(uint32_t frame_begin_ticks, bool has_avatar_marquee, bool has_active_animation,
                     bool needs_periodic_tick) const;

private:
  AppContext *context_ = nullptr;
  SceneManager scenes_;
  bool running_ = true;
  animation::TweenFloat scene_flash{0.0f};
};
