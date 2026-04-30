#include "texture_registry.h"

void TextureRegistry::Forget(SDL_Texture *texture) {
  if (!texture) return;
  sizes_.erase(texture);
}

void TextureRegistry::Remember(SDL_Texture *texture, int width, int height) {
  if (!texture) return;
  sizes_[texture] = SDL_Point{width, height};
}

void TextureRegistry::Get(SDL_Texture *texture, int &width, int &height) {
  width = 0;
  height = 0;
  if (!texture) return;

  auto it = sizes_.find(texture);
  if (it != sizes_.end()) {
    width = it->second.x;
    height = it->second.y;
    return;
  }

  SDL_QueryTexture(texture, nullptr, nullptr, &width, &height);
  Remember(texture, width, height);
}
