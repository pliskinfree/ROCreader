#include "app_shell.h"

#include <SDL.h>

#include <algorithm>

namespace {
constexpr uint32_t kActiveFrameBudgetMs = 33;
constexpr uint32_t kAvatarMarqueeFrameBudgetMs = 16;
constexpr uint32_t kPeriodicTickFrameBudgetMs = 50;
constexpr float kSceneFadeFlashAlpha = 0.82f;
constexpr float kSceneFadeFlashDurationSec = 0.18f;
}  // namespace

void AppShell::Initialize(AppContext &context) {
  context_ = &context;
  running_ = true;
}

AppContext *AppShell::Context() { return context_; }
const AppContext *AppShell::Context() const { return context_; }
SceneManager &AppShell::Scenes() { return scenes_; }
const SceneManager &AppShell::Scenes() const { return scenes_; }

bool AppShell::IsRunning() const { return running_; }

void AppShell::RequestQuit() {
  running_ = false;
}

AppFrameTiming AppShell::BeginFrame(uint32_t &previous_ticks) const {
  AppFrameTiming frame;
  frame.frame_begin_ticks = SDL_GetTicks();
  frame.now = SDL_GetTicks();
  frame.dt = std::max(0.0f, (frame.now - previous_ticks) / 1000.0f);
  previous_ticks = frame.now;
  return frame;
}

void AppShell::ResetFrameClock(uint32_t &previous_ticks) const {
  previous_ticks = SDL_GetTicks();
}

void AppShell::BeginDraw() const {
  if (!context_ || !context_->renderer) return;
  SDL_SetRenderDrawColor(context_->renderer, 26, 27, 31, 255);
  SDL_RenderClear(context_->renderer);
}

void AppShell::BeginTopDraw() const {
  if (!context_ || !context_->renderer || !context_->layout) return;
  if (context_->rgds_dual_screen && context_->rgds_bottom_renderer == context_->renderer) {
    SDL_Rect top_viewport{0, 0, context_->layout->screen_w, context_->layout->screen_h};
    SDL_RenderSetViewport(context_->renderer, &top_viewport);
  } else {
    SDL_RenderSetViewport(context_->renderer, nullptr);
  }
}

void AppShell::Present() const {
  if (!context_ || !context_->renderer) return;
  SDL_RenderSetViewport(context_->renderer, nullptr);
  if (context_->rgds_dual_screen && context_->rgds_bottom_renderer) {
    if (context_->rgds_bottom_renderer == context_->renderer) {
      SDL_RenderPresent(context_->renderer);
      return;
    }
    if (context_->rgds_bottom_renderer != context_->renderer) SDL_RenderPresent(context_->rgds_bottom_renderer);
    SDL_RenderPresent(context_->renderer);
    return;
  }
  SDL_RenderPresent(context_->renderer);
}

void AppShell::StartSceneFlash() {
  scene_flash.Snap(kSceneFadeFlashAlpha);
  scene_flash.AnimateTo(0.0f, kSceneFadeFlashDurationSec, animation::Ease::OutCubic);
}

void AppShell::TickSceneFlash(float dt, bool animate_enabled) {
  if (animate_enabled) scene_flash.Update(dt);
  else scene_flash.Snap(0.0f);
}

bool AppShell::IsSceneFlashAnimating() const { return scene_flash.IsAnimating(); }

void AppShell::DrawSceneFlash() const {
  if (!context_ || !context_->renderer || !context_->layout) return;
  const float flash = scene_flash.Value();
  if (flash <= 0.001f) return;
  SDL_SetRenderDrawBlendMode(context_->renderer, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(context_->renderer, 0, 0, 0,
                         static_cast<Uint8>(std::clamp(flash, 0.0f, 1.0f) * 255.0f));
  SDL_Rect rect{0, 0, context_->layout->screen_w, context_->layout->screen_h};
  SDL_RenderFillRect(context_->renderer, &rect);
}

AppEventPumpResult AppShell::PumpEvents(InputManager &input, bool active, int idle_wait_ms,
                                        const std::function<void(const SDL_Event &)> &note_user_input) {
  AppEventPumpResult result;
  SDL_Event event;
  auto handle_event = [&](const SDL_Event &e) {
    if (e.type == SDL_QUIT) RequestQuit();
    if (note_user_input) note_user_input(e);
    input.HandleEvent(e);
  };

  if (active) {
    while (SDL_PollEvent(&event)) {
      result.had_event = true;
      handle_event(event);
    }
    return result;
  }

  if (SDL_WaitEventTimeout(&event, idle_wait_ms)) {
    result.had_event = true;
    handle_event(event);
    while (SDL_PollEvent(&event)) {
      handle_event(event);
    }
  } else {
    result.timed_out = true;
  }
  return result;
}

void AppShell::ThrottleFrame(uint32_t frame_begin_ticks, bool has_avatar_marquee, bool has_active_animation,
                             bool needs_periodic_tick) const {
  uint32_t frame_budget_ms = 0;
  if (has_avatar_marquee) frame_budget_ms = kAvatarMarqueeFrameBudgetMs;
  else if (has_active_animation) frame_budget_ms = kActiveFrameBudgetMs;
  else if (needs_periodic_tick) frame_budget_ms = kPeriodicTickFrameBudgetMs;
  if (frame_budget_ms > 0) {
    const uint32_t frame_elapsed = SDL_GetTicks() - frame_begin_ticks;
    if (frame_elapsed < frame_budget_ms) SDL_Delay(frame_budget_ms - frame_elapsed);
  }
}
