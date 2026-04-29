#pragma once

#include <SDL.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

struct AsyncImageRenderJob {
  bool active = false;
  bool prefetch = false;
  std::string source_key;
  int page = 0;
  float scale = 1.0f;
  int rotation = 0;
  float zoom = 1.0f;
  int scroll_x = 0;
  int scroll_y = 0;
  int viewport_w = 0;
  int viewport_h = 0;
  uint64_t serial = 0;
};

struct AsyncImageRenderResult {
  bool ready = false;
  bool success = false;
  AsyncImageRenderJob job;
  int width = 0;
  int height = 0;
  std::vector<unsigned char> rgba;
};

using AsyncImageRenderFn =
    std::function<bool(const AsyncImageRenderJob &, std::atomic<bool> &, AsyncImageRenderResult &)>;

class AsyncImageRenderQueue {
public:
  AsyncImageRenderQueue();
  ~AsyncImageRenderQueue();

  AsyncImageRenderQueue(const AsyncImageRenderQueue &) = delete;
  AsyncImageRenderQueue &operator=(const AsyncImageRenderQueue &) = delete;

  bool Start(std::string thread_name, AsyncImageRenderFn render_fn);
  void Shutdown();
  void Reset();

  bool Request(AsyncImageRenderJob job, bool allow_prefetch);
  bool TakeReady(AsyncImageRenderResult &out_result);
  bool IsBusyOrReady() const;
  void CancelTarget();

private:
  struct Impl;
  static int WorkerMain(void *userdata);
  Impl *impl_ = nullptr;
};
