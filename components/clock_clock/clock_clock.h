#pragma once

#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/core/color.h"
#include "esphome/components/time/real_time_clock.h"
#include "esphome/components/display/display.h"

#ifdef USE_LVGL
#include <lvgl.h>
#endif

namespace esphome {
namespace clock_clock {

// A "ClockClock 24" style display: a digital clock rendered out of 24 tiny
// analogue clocks (4 digits x 2 columns x 3 rows). Each little clock has two
// hands of equal length. By pointing the hands the right way the clocks form
// the strokes of the digits. Hands are animated (always sweeping clockwise)
// from their previous position to the new target whenever the time changes.
//
// It is a drawable helper (not a display driver). Add a `display:` of your own
// and call `id(my_clock_clock).draw(it);` from its lambda, just like `graph`.

static const int NUM_DIGITS = 4;
static const int CLOCKS_PER_DIGIT = 6;  // 2 columns x 3 rows
static const int NUM_CLOCKS = NUM_DIGITS * CLOCKS_PER_DIGIT;  // 24
static const int NUM_HANDS = NUM_CLOCKS * 2;                  // 48

enum ClockClockMode {
  CC_MODE_TIME = 0,     // show the clock
  CC_MODE_ROTATE_LEFT,  // all hands spin anti-clockwise forever
  CC_MODE_FLYING_BIRDS,  // every clock is a flapping seagull silhouette
};

enum MovementMode {
  CC_MOVE_OPPOSITE = 0,  // the two hands counter-rotate (the ClockClock look)
  CC_MOVE_CLOCKWISE,     // both hands take the shortest clockwise path
  CC_MOVE_COUNTER,       // both hands take the shortest anti-clockwise path
  CC_MOVE_LONG,          // both hands take the long way round (> 180 deg)
};

class ClockClock : public Component {
 public:
  void set_time(time::RealTimeClock *t) { this->time_ = t; }
  void set_transition_length(uint32_t ms) { this->transition_ms_ = ms; }
  void set_spacing(float clocks) { this->spacing_ = clocks; }
  void set_hand_width(int px) { this->hand_width_ = px; }
  void set_show_face(bool show) { this->show_face_ = show; }
  void set_twenty_four_hour(bool on) { this->twenty_four_hour_ = on; }
  void set_mode_speed(float s) { this->mode_speed_ = s; }
  void set_movement(MovementMode m) { this->movement_ = m; }
  // Placement inside the display for the no-argument draw(it). width/height of 0
  // means "fill to the edge of the display".
  void set_position(int x, int y) {
    this->cfg_x_ = x;
    this->cfg_y_ = y;
  }
  void set_size(int w, int h) {
    this->cfg_w_ = w;
    this->cfg_h_ = h;
  }

  // Colours (for grayscale / RGB panels; ignored-but-harmless on mono, which
  // only distinguishes on/off). Unset roles fall back: pointer/border -> the
  // foreground, clock face -> the background.
  void set_foreground(Color c) { this->fg_ = c; }
  void set_background(Color c) { this->background_ = c; }
  void set_pointer_color(Color c) {
    this->pointer_ = c;
    this->has_pointer_ = true;
  }
  void set_border_color(Color c) {
    this->border_ = c;
    this->has_border_ = true;
  }
  void set_clock_bg_color(Color c) {
    this->clock_bg_ = c;
    this->has_clock_bg_ = true;
  }

  // Switch the running mode. Drive these from YAML actions / lambdas, e.g.
  // rotate while Wi-Fi connects, flying-birds while waiting for time sync, then
  // show_time once the clock is set.
  void set_mode(ClockClockMode m);
  void set_time_mode() { this->set_mode(CC_MODE_TIME); }
  void set_rotate_left_mode() { this->set_mode(CC_MODE_ROTATE_LEFT); }
  void set_flying_birds_mode() { this->set_mode(CC_MODE_FLYING_BIRDS); }
  ClockClockMode get_mode() const { return this->mode_; }

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  // Draw filling the whole display.
  void draw(display::Display &disp);
  // Draw into an explicit rectangle (auto-scales the grid to fit).
  void draw(display::Display &disp, int x, int y, int w, int h);
  void draw(display::Display &disp, int x, int y, int w, int h, Color color);

#ifdef USE_LVGL
  // Draw into an LVGL canvas widget (LVGL 9). The grid fills the canvas. Call
  // from an interval or an lvgl trigger lambda:
  //   id(cc).draw_to_canvas(id(my_canvas));
  void draw_to_canvas(lv_obj_t *canvas);
#endif

  // Smallest width/height (in px) that still renders legibly for the current
  // configuration. Useful for logging / sanity checks.
  int min_width() const;
  int min_height() const;

 protected:
  void set_time_(int hh, int mm);
  void retarget_();
  void tick_time_(uint32_t now_ms);
  void tick_rotate_(uint32_t now_ms);
  void tick_birds_(uint32_t now_ms);
  void render_(display::Display &disp, int x, int y, int w, int h, Color pointer);
  void draw_hand_(display::Display &disp, int cx, int cy, int len, float angle_deg, Color color);
#ifdef USE_LVGL
  void canvas_hand_(lv_layer_t *layer, lv_draw_line_dsc_t *dsc, int cx, int cy, int len,
                    float angle_deg);
#endif

  Color pointer_color_() const { return this->has_pointer_ ? this->pointer_ : this->fg_; }
  Color border_color_() const { return this->has_border_ ? this->border_ : this->fg_; }
  Color clock_bg_color_() const { return this->has_clock_bg_ ? this->clock_bg_ : this->background_; }

  time::RealTimeClock *time_{nullptr};
  uint32_t transition_ms_{2000};
  float spacing_{0.6f};   // gap between HH and MM, in clock widths
  int hand_width_{1};     // hand thickness in px (1 is right for ~128x64; bump on bigger panels)
  bool show_face_{false};
  bool twenty_four_hour_{true};
  MovementMode movement_{CC_MOVE_OPPOSITE};
  float mode_speed_{1.0f};  // speed multiplier for the rotate / birds modes

  Color fg_{Color(255, 255, 255)};          // foreground default (mono: on)
  Color background_{Color(0, 0, 0)};        // around the clocks (mono: off)
  Color pointer_{};
  Color border_{};
  Color clock_bg_{};
  bool has_pointer_{false};
  bool has_border_{false};
  bool has_clock_bg_{false};

  ClockClockMode mode_{CC_MODE_TIME};

  int cfg_x_{0};
  int cfg_y_{0};
  int cfg_w_{0};  // 0 => fill to display edge
  int cfg_h_{0};  // 0 => fill to display edge
  bool size_checked_{false};

  int digits_[NUM_DIGITS]{-1, -1, -1, -1};
  int last_key_{-1};

  float cur_[NUM_HANDS];     // current angle (deg, may grow past 360 mid-sweep)
  float start_[NUM_HANDS];   // angle at the start of the sweep
  float target_[NUM_HANDS];  // angle at the end of the sweep
  bool animating_{false};
  uint32_t anim_start_{0};
};

// Action for YAML automations: `clock_clock.show_time` / `.rotate_left` /
// `.flying_birds`. Each is registered with a fixed target mode.
template<typename... Ts> class SetModeAction : public Action<Ts...> {
 public:
  SetModeAction(ClockClock *parent, ClockClockMode mode) : parent_(parent), mode_(mode) {}
  void play(Ts... x) override { this->parent_->set_mode(this->mode_); }

 protected:
  ClockClock *parent_;
  ClockClockMode mode_;
};

}  // namespace clock_clock
}  // namespace esphome
