#pragma once

#include "input_manager.h"

#include <SDL.h>

class IMenuPanel {
public:
  virtual ~IMenuPanel() = default;

  virtual void OnEnter() {}
  virtual void OnExit() {}
  virtual bool HandleInput(const InputManager &input, float dt) = 0;
  virtual void Draw(SDL_Renderer *renderer) = 0;
};
