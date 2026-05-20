#include "async_image_render_queue.h"

#include "image_runtime_tuning.h"

#include <cmath>
#include <utility>

struct AsyncImageRenderQueue::Impl {
  SDL_mutex *mutex = nullptr;
  SDL_cond *cond = nullptr;
  SDL_Thread *thread = nullptr;
  bool running = false;
  AsyncImageRenderJob requested;
  AsyncImageRenderJob inflight;
  AsyncImageRenderResult result;
  uint64_t job_serial = 0;
  uint64_t latest_target_serial = 0;
  std::atomic<bool> cancel_requested{false};
  AsyncImageRenderFn render_fn;
  std::string thread_name;
};

namespace {

bool SameJobVisualTarget(const AsyncImageRenderJob &a, const AsyncImageRenderJob &b) {
  return a.active && b.active &&
         a.source_key == b.source_key &&
         a.page == b.page &&
         a.rotation == b.rotation &&
         std::abs(a.zoom - b.zoom) < 0.0005f &&
         a.scroll_x == b.scroll_x &&
         a.scroll_y == b.scroll_y &&
         std::abs(a.scale - b.scale) < 0.0005f;
}

bool RequestedDiffersFromJob(const AsyncImageRenderJob &requested,
                             const AsyncImageRenderJob &job) {
  return requested.active &&
         (requested.source_key != job.source_key ||
          requested.page != job.page ||
          requested.rotation != job.rotation ||
          std::abs(requested.zoom - job.zoom) >= 0.0005f ||
          requested.scroll_x != job.scroll_x ||
          requested.scroll_y != job.scroll_y ||
          std::abs(requested.scale - job.scale) >= 0.0005f);
}

}  // namespace

int AsyncImageRenderQueue::WorkerMain(void *userdata) {
  auto *impl = static_cast<AsyncImageRenderQueue::Impl *>(userdata);
  SDL_SetThreadPriority(image_runtime_tuning::NormalRenderThreadPriority()
                            ? SDL_THREAD_PRIORITY_NORMAL
                            : SDL_THREAD_PRIORITY_LOW);

  for (;;) {
    SDL_LockMutex(impl->mutex);
    while (impl->running && (!impl->requested.active || impl->result.ready)) {
      SDL_CondWait(impl->cond, impl->mutex);
    }
    if (!impl->running) {
      SDL_UnlockMutex(impl->mutex);
      break;
    }

    AsyncImageRenderJob job = impl->requested;
    impl->requested = AsyncImageRenderJob{};
    impl->inflight = job;
    if (!job.prefetch) impl->cancel_requested.store(false);
    SDL_UnlockMutex(impl->mutex);

    AsyncImageRenderResult rendered;
    rendered.job = job;
    bool success = false;
    if (impl->render_fn) {
      success = impl->render_fn(job, impl->cancel_requested, rendered);
    }
    rendered.ready = true;
    rendered.success = success;
    rendered.job = job;

    SDL_LockMutex(impl->mutex);
    const bool stale =
        RequestedDiffersFromJob(impl->requested, job) ||
        (!job.prefetch && job.serial != impl->latest_target_serial) ||
        (job.prefetch && job.serial < impl->latest_target_serial) ||
        impl->cancel_requested.load();
    if (impl->running && !stale) {
      impl->result = std::move(rendered);
    }
    impl->inflight = AsyncImageRenderJob{};
    SDL_UnlockMutex(impl->mutex);
  }

  return 0;
}

AsyncImageRenderQueue::AsyncImageRenderQueue() : impl_(new Impl()) {}

AsyncImageRenderQueue::~AsyncImageRenderQueue() {
  Shutdown();
  delete impl_;
  impl_ = nullptr;
}

bool AsyncImageRenderQueue::Start(std::string thread_name, AsyncImageRenderFn render_fn) {
  if (!impl_ || impl_->thread) return false;
  impl_->mutex = SDL_CreateMutex();
  impl_->cond = SDL_CreateCond();
  impl_->render_fn = std::move(render_fn);
  impl_->thread_name = std::move(thread_name);
  if (!impl_->mutex || !impl_->cond || !impl_->render_fn) {
    Shutdown();
    return false;
  }

  impl_->running = true;
  impl_->thread = SDL_CreateThread(AsyncImageRenderQueue::WorkerMain,
                                   impl_->thread_name.empty() ? "async_image_render" : impl_->thread_name.c_str(),
                                   impl_);
  if (!impl_->thread) {
    impl_->running = false;
    Shutdown();
    return false;
  }
  return true;
}

void AsyncImageRenderQueue::Shutdown() {
  if (!impl_) return;
  if (impl_->mutex) {
    SDL_LockMutex(impl_->mutex);
    impl_->running = false;
    impl_->cancel_requested.store(true);
    if (impl_->cond) SDL_CondSignal(impl_->cond);
    SDL_UnlockMutex(impl_->mutex);
  }
  if (impl_->thread) {
    SDL_WaitThread(impl_->thread, nullptr);
    impl_->thread = nullptr;
  }
  Reset();
  if (impl_->cond) SDL_DestroyCond(impl_->cond);
  if (impl_->mutex) SDL_DestroyMutex(impl_->mutex);
  impl_->cond = nullptr;
  impl_->mutex = nullptr;
  impl_->render_fn = nullptr;
  impl_->thread_name.clear();
}

void AsyncImageRenderQueue::Reset() {
  if (!impl_) return;
  if (impl_->mutex) SDL_LockMutex(impl_->mutex);
  impl_->requested = AsyncImageRenderJob{};
  impl_->inflight = AsyncImageRenderJob{};
  impl_->result = AsyncImageRenderResult{};
  impl_->job_serial = 0;
  impl_->latest_target_serial = 0;
  impl_->cancel_requested.store(false);
  if (impl_->mutex) SDL_UnlockMutex(impl_->mutex);
}

bool AsyncImageRenderQueue::Request(AsyncImageRenderJob job, bool allow_prefetch) {
  if (!impl_ || !impl_->mutex || !impl_->running) return false;
  job.active = true;
  job.serial = ++impl_->job_serial;
  if (job.serial == 0) job.serial = ++impl_->job_serial;

  SDL_LockMutex(impl_->mutex);
  if (!job.prefetch) {
    impl_->latest_target_serial = job.serial;
    impl_->cancel_requested.store(true);
  }

  const bool busy_with_target =
      impl_->requested.active || (impl_->inflight.active && !impl_->inflight.prefetch);
  const bool allow_request =
      job.prefetch ? (allow_prefetch && !busy_with_target && !impl_->inflight.active) : true;
  const bool duplicate =
      SameJobVisualTarget(impl_->requested, job) || SameJobVisualTarget(impl_->inflight, job);

  bool accepted = false;
  if (allow_request && !duplicate) {
    impl_->requested = std::move(job);
    if (impl_->cond) SDL_CondSignal(impl_->cond);
    accepted = true;
  }
  SDL_UnlockMutex(impl_->mutex);
  return accepted;
}

bool AsyncImageRenderQueue::TakeReady(AsyncImageRenderResult &out_result) {
  if (!impl_ || !impl_->mutex) return false;
  bool has_result = false;
  SDL_LockMutex(impl_->mutex);
  if (impl_->result.ready) {
    out_result = std::move(impl_->result);
    impl_->result = AsyncImageRenderResult{};
    has_result = true;
    if (impl_->cond) SDL_CondSignal(impl_->cond);
  }
  SDL_UnlockMutex(impl_->mutex);
  return has_result;
}

bool AsyncImageRenderQueue::IsBusyOrReady() const {
  if (!impl_ || !impl_->mutex) return false;
  SDL_LockMutex(impl_->mutex);
  const bool busy = impl_->requested.active || impl_->inflight.active || impl_->result.ready;
  SDL_UnlockMutex(impl_->mutex);
  return busy;
}

void AsyncImageRenderQueue::CancelTarget() {
  if (!impl_ || !impl_->mutex) return;
  SDL_LockMutex(impl_->mutex);
  impl_->latest_target_serial = ++impl_->job_serial;
  impl_->cancel_requested.store(true);
  SDL_UnlockMutex(impl_->mutex);
}
