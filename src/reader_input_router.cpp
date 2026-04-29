#include "reader_input_router.h"

#include "reader_core.h"

#include <SDL.h>

#include <string>

namespace {
int EpubScrollDirForButton(int rotation, Button button) {
  if (rotation == 0) {
    if (button == Button::Down) return 1;
    if (button == Button::Up) return -1;
  } else if (rotation == 90) {
    if (button == Button::Left) return 1;
    if (button == Button::Right) return -1;
  } else if (rotation == 180) {
    if (button == Button::Up) return 1;
    if (button == Button::Down) return -1;
  } else {
    if (button == Button::Left) return -1;
    if (button == Button::Right) return 1;
  }
  return 0;
}

int EpubTapPageActionForButton(int rotation, Button button) {
  if (rotation == 0) {
    if (button == Button::Right) return 1;
    if (button == Button::Left) return -1;
  } else if (rotation == 90) {
    if (button == Button::Up) return -1;
    if (button == Button::Down) return 1;
  } else if (rotation == 180) {
    if (button == Button::Left) return 1;
    if (button == Button::Right) return -1;
  } else {
    if (button == Button::Up) return 1;
    if (button == Button::Down) return -1;
  }
  return 0;
}
}  // namespace

void HandleReaderInput(ReaderInputRouterDeps &deps) {
  ReaderMode &reader_mode = deps.ui.mode;
  TxtReaderState &txt_reader = deps.ui.Txt();
  bool &reader_progress_overlay_visible = deps.ui.progress_overlay_visible;
  auto &long_fired = deps.ui.long_fired;
  auto &hold_speed = deps.ui.hold_speed;

  if (deps.input.IsJustPressed(Button::X)) {
    reader_progress_overlay_visible = !reader_progress_overlay_visible;
    if (reader_progress_overlay_visible) {
      const int pct = deps.current_reader_progress_pct ? deps.current_reader_progress_pct() : 0;
      deps.ui.progress_overlay_preview_pct = pct;
      deps.ui.progress_overlay_preview_pct_f = static_cast<float>(pct);
      deps.ui.progress_overlay_dirty = false;
      deps.ui.progress_overlay_scrubbing = false;
    } else {
      deps.ui.progress_overlay_dirty = false;
      deps.ui.progress_overlay_scrubbing = false;
    }
  }

  if (reader_progress_overlay_visible &&
      ((reader_mode == ReaderMode::Txt && txt_reader.open) ||
       (reader_mode == ReaderMode::Pdf && deps.pdf_runtime.IsOpen()) ||
       (reader_mode == ReaderMode::Epub && deps.epub_runtime.IsOpen()) ||
       (reader_mode == ReaderMode::ZipImage && deps.zip_image_runtime.IsOpen()))) {
    TxtProgressOverlayInputDeps txt_overlay_input_deps{
        deps.input,
        deps.ui,
        deps.dt,
        deps.current_reader_progress_pct ? deps.current_reader_progress_pct() : 0,
        !(reader_mode == ReaderMode::Txt && txt_reader.open && txt_reader.loading),
        deps.progress_overlay_tap_step_pct,
        deps.progress_overlay_hold_delay_sec,
        deps.progress_overlay_hold_speed_min_pct,
        deps.progress_overlay_hold_speed_max_pct,
        deps.progress_overlay_hold_accel_pct,
        deps.reader_jump_to_percent,
    };
    HandleTxtProgressOverlayInput(txt_overlay_input_deps);
  } else if (reader_mode == ReaderMode::Txt && txt_reader.open) {
    TxtReaderInputDeps txt_input_deps{
        deps.input,
        deps.ui,
        deps.dt,
        deps.tap_step_px,
        deps.text_scroll_by,
        deps.text_page_by,
    };
    HandleTxtReaderInput(txt_input_deps);
  } else if (reader_mode == ReaderMode::Pdf) {
    const int pdf_rotation = deps.pdf_runtime.Progress().rotation;
    const auto pdf_progress = deps.pdf_runtime.Progress();
    const bool pdf_zoomed = pdf_progress.zoom > 1.0005f;
    auto pdf_pan_delta_for_page_button = [&](Button button) -> int {
      const int page_action = PdfTapPageActionForButton(pdf_rotation, button);
      if (page_action == 0) return 0;
      const int pan_sign = (pdf_rotation == 180 || pdf_rotation == 270) ? -page_action : page_action;
      return pan_sign * deps.tap_step_px;
    };
    if (deps.input.IsJustPressed(Button::L2)) {
      deps.pdf_runtime.RotateLeft();
    }
    if (deps.input.IsJustPressed(Button::R2)) {
      deps.pdf_runtime.RotateRight();
    }
    if (deps.input.IsJustPressed(Button::L1)) {
      deps.pdf_runtime.ZoomOut();
    }
    if (deps.input.IsJustPressed(Button::R1)) {
      deps.pdf_runtime.ZoomIn();
    }
    if (deps.input.IsJustPressed(Button::A)) {
      deps.pdf_runtime.ResetView();
    }

    std::array<Button, 4> dirs = {Button::Up, Button::Down, Button::Left, Button::Right};
    for (Button b : dirs) {
      int bi = static_cast<int>(b);
      int long_dir = PdfScrollDirForButton(pdf_rotation, b);
      if (long_dir == 0) {
        if (pdf_zoomed) {
          const int pan_delta = pdf_pan_delta_for_page_button(b);
          if (pan_delta != 0 && deps.input.IsPressed(b) && deps.input.HoldTime(b) >= 0.28f) {
            long_fired[bi] = true;
            deps.pdf_runtime.PanHorizontalByPixels(pan_delta > 0 ? 20 : -20);
          }
        }
        hold_speed[bi] = 0.0f;
        continue;
      }
      if (deps.input.IsPressed(b) && deps.input.HoldTime(b) >= 0.28f) {
        long_fired[bi] = true;
        deps.pdf_runtime.ScrollByPixels(long_dir * 20);
      } else if (!deps.input.IsPressed(b)) {
        hold_speed[bi] = 0.0f;
      }
    }

    for (Button b : dirs) {
      int bi = static_cast<int>(b);
      if (!deps.input.IsJustReleased(b)) continue;
      if (long_fired[bi]) {
        long_fired[bi] = false;
        continue;
      }
      const int tap_dir = PdfScrollDirForButton(pdf_rotation, b);
      if (tap_dir != 0) {
        deps.pdf_runtime.ScrollByPixels(tap_dir * 60);
      } else {
        if (pdf_zoomed) {
          const int pan_delta = pdf_pan_delta_for_page_button(b);
          if (pan_delta != 0) {
            deps.pdf_runtime.PanHorizontalByPixels(pan_delta);
            continue;
          }
        }
        const int page_action = PdfTapPageActionForButton(pdf_rotation, b);
        if (page_action != 0) {
          deps.pdf_runtime.JumpByScreen(page_action);
        }
      }
    }
  } else if (reader_mode == ReaderMode::Epub) {
    const int epub_rotation = deps.epub_runtime.Progress().rotation;
    const auto epub_progress = deps.epub_runtime.Progress();
    const bool flow_epub = std::string(deps.epub_runtime.BackendName()) == "epub-flow";
    const bool epub_zoomed = !flow_epub && epub_progress.zoom > 1.0005f;
    auto epub_pan_delta_for_page_button = [&](Button button) -> int {
      const int page_action = EpubTapPageActionForButton(epub_rotation, button);
      if (page_action == 0) return 0;
      const int pan_sign = (epub_rotation == 180 || epub_rotation == 270) ? -page_action : page_action;
      return pan_sign * deps.tap_step_px;
    };
    const bool rotate_left_pressed = deps.input.IsJustPressed(Button::L2);
    const bool rotate_right_pressed = deps.input.IsJustPressed(Button::R2);
    const bool zoom_out_pressed = deps.input.IsJustPressed(Button::L1);
    const bool zoom_in_pressed = deps.input.IsJustPressed(Button::R1);
    if (flow_epub && (rotate_left_pressed || rotate_right_pressed || zoom_out_pressed || zoom_in_pressed)) {
      if (!deps.transient_message_dismissed_this_frame && deps.show_transient_message) {
        deps.show_transient_message("EPUB flow mode disables rotate and zoom", 3000, true);
      }
    } else {
      if (rotate_left_pressed) {
        deps.epub_runtime.RotateLeft();
      }
      if (rotate_right_pressed) {
        deps.epub_runtime.RotateRight();
      }
      if (zoom_out_pressed) {
        deps.epub_runtime.ZoomOut();
      }
      if (zoom_in_pressed) {
        deps.epub_runtime.ZoomIn();
      }
    }
    if (deps.input.IsJustPressed(Button::A)) {
      deps.epub_runtime.ResetView();
    }

    if (flow_epub) {
      std::array<Button, 2> flow_dirs = {Button::Up, Button::Down};
      for (Button b : flow_dirs) {
        int bi = static_cast<int>(b);
        const int long_dir = (b == Button::Down) ? 1 : -1;
        if (deps.input.IsPressed(b) && deps.input.HoldTime(b) >= 0.28f) {
          long_fired[bi] = true;
          deps.epub_runtime.ScrollByPixels(long_dir * 20);
        } else if (!deps.input.IsPressed(b)) {
          hold_speed[bi] = 0.0f;
        }
      }

      for (Button b : flow_dirs) {
        int bi = static_cast<int>(b);
        if (!deps.input.IsJustReleased(b)) continue;
        if (long_fired[bi]) {
          long_fired[bi] = false;
          continue;
        }
        const int tap_dir = (b == Button::Down) ? 1 : -1;
        deps.epub_runtime.ScrollByPixels(tap_dir * 60);
      }

      if (deps.input.IsJustPressed(Button::Right)) {
        deps.epub_runtime.JumpByScreen(1);
      } else if (deps.input.IsJustPressed(Button::Left)) {
        deps.epub_runtime.JumpByScreen(-1);
      }
    } else {
      std::array<Button, 4> dirs = {Button::Up, Button::Down, Button::Left, Button::Right};
      for (Button b : dirs) {
        int bi = static_cast<int>(b);
        int long_dir = EpubScrollDirForButton(epub_rotation, b);
        if (long_dir == 0) {
          if (epub_zoomed) {
            const int pan_delta = epub_pan_delta_for_page_button(b);
            if (pan_delta != 0 && deps.input.IsPressed(b) && deps.input.HoldTime(b) >= 0.28f) {
              long_fired[bi] = true;
              deps.epub_runtime.PanHorizontalByPixels(pan_delta > 0 ? 20 : -20);
            }
          }
          hold_speed[bi] = 0.0f;
          continue;
        }
        if (deps.input.IsPressed(b) && deps.input.HoldTime(b) >= 0.28f) {
          long_fired[bi] = true;
          deps.epub_runtime.ScrollByPixels(long_dir * 20);
        } else if (!deps.input.IsPressed(b)) {
          hold_speed[bi] = 0.0f;
        }
      }

      for (Button b : dirs) {
        int bi = static_cast<int>(b);
        if (!deps.input.IsJustReleased(b)) continue;
        if (long_fired[bi]) {
          long_fired[bi] = false;
          continue;
        }
        const int tap_dir = EpubScrollDirForButton(epub_rotation, b);
        if (tap_dir != 0) {
          deps.epub_runtime.ScrollByPixels(tap_dir * 60);
        } else {
          if (epub_zoomed) {
            const int pan_delta = epub_pan_delta_for_page_button(b);
            if (pan_delta != 0) {
              deps.epub_runtime.PanHorizontalByPixels(pan_delta);
              continue;
            }
          }
          const int page_action = EpubTapPageActionForButton(epub_rotation, b);
          if (page_action != 0) {
            deps.epub_runtime.JumpByScreen(page_action);
          }
        }
      }
    }
  } else if (reader_mode == ReaderMode::ZipImage) {
    const int zip_rotation = deps.zip_image_runtime.Progress().rotation;
    const auto zip_progress = deps.zip_image_runtime.Progress();
    const bool zip_zoomed = zip_progress.zoom > 1.0005f;
    auto zip_pan_delta_for_page_button = [&](Button button) -> int {
      const int page_action = EpubTapPageActionForButton(zip_rotation, button);
      if (page_action == 0) return 0;
      const int pan_sign = (zip_rotation == 180 || zip_rotation == 270) ? -page_action : page_action;
      return pan_sign * deps.tap_step_px;
    };
    if (deps.input.IsJustPressed(Button::L2)) {
      deps.zip_image_runtime.RotateLeft();
    }
    if (deps.input.IsJustPressed(Button::R2)) {
      deps.zip_image_runtime.RotateRight();
    }
    if (deps.input.IsJustPressed(Button::L1)) {
      deps.zip_image_runtime.ZoomOut();
    }
    if (deps.input.IsJustPressed(Button::R1)) {
      deps.zip_image_runtime.ZoomIn();
    }
    if (deps.input.IsJustPressed(Button::A)) {
      deps.zip_image_runtime.ResetView();
    }

    std::array<Button, 4> dirs = {Button::Up, Button::Down, Button::Left, Button::Right};
    for (Button b : dirs) {
      int bi = static_cast<int>(b);
      int long_dir = EpubScrollDirForButton(zip_rotation, b);
      if (long_dir == 0) {
        if (zip_zoomed) {
          const int pan_delta = zip_pan_delta_for_page_button(b);
          if (pan_delta != 0 && deps.input.IsPressed(b) && deps.input.HoldTime(b) >= 0.28f) {
            long_fired[bi] = true;
            deps.zip_image_runtime.PanHorizontalByPixels(pan_delta > 0 ? 20 : -20);
          }
        }
        hold_speed[bi] = 0.0f;
        continue;
      }
      if (deps.input.IsPressed(b) && deps.input.HoldTime(b) >= 0.28f) {
        long_fired[bi] = true;
        deps.zip_image_runtime.ScrollByPixels(long_dir * 20);
      } else if (!deps.input.IsPressed(b)) {
        hold_speed[bi] = 0.0f;
      }
    }

    for (Button b : dirs) {
      int bi = static_cast<int>(b);
      if (!deps.input.IsJustReleased(b)) continue;
      if (long_fired[bi]) {
        long_fired[bi] = false;
        continue;
      }
      const int tap_dir = EpubScrollDirForButton(zip_rotation, b);
      if (tap_dir != 0) {
        deps.zip_image_runtime.ScrollByPixels(tap_dir * 60);
      } else {
        if (zip_zoomed) {
          const int pan_delta = zip_pan_delta_for_page_button(b);
          if (pan_delta != 0) {
            deps.zip_image_runtime.PanHorizontalByPixels(pan_delta);
            continue;
          }
        }
        const int page_action = EpubTapPageActionForButton(zip_rotation, b);
        if (page_action != 0) {
          deps.zip_image_runtime.JumpByScreen(page_action);
        }
      }
    }
  }
}

