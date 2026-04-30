#include "boot_scene.h"

#include "runtime_log.h"

#include <cstdlib>
#include <iostream>
#include <utility>

BootScene::BootScene(BootRuntimeState &runtime, std::function<void()> request_quit)
    : runtime_(runtime), request_quit_(std::move(request_quit)) {}

void BootScene::Tick(float dt, const BootSceneTickDeps &deps) {
  BootRuntimeTickDeps runtime_deps{
      deps.books_roots,
      deps.count_batch_entries,
      deps.scan_batch_entries,
      deps.cover_generate_batch_entries,
      deps.get_lower_ext,
      deps.doc_cover_backend_available,
      deps.has_manual_cover_exact_or_fuzzy,
      deps.has_cached_doc_cover_on_disk,
      deps.create_doc_first_page_cover_texture,
      deps.destroy_generated_cover_texture,
      deps.install_pending_update,
      deps.on_update_installed_restart,
      deps.on_finish,
  };
  TickBootRuntime(runtime_, dt, runtime_deps);
}

void BootScene::Draw(const BootSceneRenderDeps &deps) const {
  BootRuntimeRenderDeps runtime_deps{
      deps.renderer,
      runtime_,
      deps.language_index,
      deps.screen_w,
      deps.screen_h,
      deps.draw_rect,
      deps.get_text_texture,
  };
  DrawBootRuntime(runtime_deps);
}

bool BootScene::InstallPendingUpdateFromEnvironment() const {
  const char *command = std::getenv("ROCREADER_UPDATE_INSTALL_COMMAND");
  if (!command || !*command) return false;

  runtime_log::Line("boot: pending update install begin");
  const int rc = std::system(command);
  const bool ok = (rc == 0);
  runtime_log::Line(std::string("boot: pending update install ") + (ok ? "success" : "failed") +
                    " rc=" + std::to_string(rc));
  return ok;
}

void BootScene::RestartAfterInstalledUpdate() const {
  runtime_log::Line("boot: pending update installed; restart via launcher");
  if (request_quit_) request_quit_();
  std::exit(23);
}

void BootScene::ResetShelfAfterBoot(BootSceneShelfResetDeps deps) const {
  ShelfSceneState &state = deps.shelf_state;
  state.current_folder.clear();
  state.nav_selected_index = 0;
  state.focus_index = 0;
  state.shelf_page = 0;
  state.page_animating = false;
  state.page_slide.Snap(0.0f);
  state.grid_item_anims.clear();
  state.title_focus_index = -1;
  state.title_marquee_active = false;
  state.title_marquee_offset = 0.0f;
  state.title_marquee_wait = deps.title_marquee_pause_sec;
}

void BootScene::FinishScanAndEnterShelf(size_t total_books,
                                        size_t cover_generate_count,
                                        BootSceneFinishDeps deps) const {
  if (deps.rebuild_shelf_items) deps.rebuild_shelf_items();
  ResetShelfAfterBoot(deps.shelf_reset);

  runtime_log::Line(std::string("boot: scan complete books=") + std::to_string(total_books) +
                    " cover_generate=" + std::to_string(cover_generate_count));
  if (deps.verbose_log) {
    std::cout << "[native_h700] boot scan complete: books=" << total_books
              << " cover_generate=" << cover_generate_count << "\n";
  }
  if (deps.enter_shelf) deps.enter_shelf();
}
