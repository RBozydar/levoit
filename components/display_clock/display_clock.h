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
namespace display_clock {

// A clock you draw onto any ESPHome display. Pick a `style`:
//   - clockclock24 : a digital clock made of 24 tiny analogue clocks
//   - analog       : one classic analogue clock face
//   - digital      : HH:MM(:SS) rendered with a font
// It is a drawable helper (not a display driver): call `id(dc).draw(it);` from a
// display lambda, like the `graph` component.

enum ClockStyle {
  STYLE_CLOCKCLOCK24 = 0,
  STYLE_ANALOG,
  STYLE_DIGITAL,
};

// clockclock24-only: idle animations, driven from YAML actions.
enum ClockMode {
  CC_MODE_TIME = 0,
  CC_MODE_ROTATE_LEFT,
  CC_MODE_FLYING_BIRDS,
};

// clockclock24-only: how the two hands travel to a new digit.
enum MovementMode {
  CC_MOVE_OPPOSITE = 0,
  CC_MOVE_CLOCKWISE,
  CC_MOVE_COUNTER,
  CC_MOVE_LONG,
};

static const int NUM_DIGITS = 4;
static const int CLOCKS_PER_DIGIT = 6;
static const int NUM_CLOCKS = NUM_DIGITS * CLOCKS_PER_DIGIT;  // 24
static const int NUM_HANDS = NUM_CLOCKS * 2;                  // 48

// One glyph of the digital layout. val: 0-9 = digit, 10 = colon (on),
// 11 = colon (blinked off), -1 = blank space, -2 = dash ("--:--").
struct DigitalCell {
  int val;
  int x, y, w, h;
};

class DisplayClock : public Component {
 public:
  // --- shared ---
  void set_time(time::RealTimeClock *t) { this->time_ = t; }
  void set_style(ClockStyle s) { this->style_ = s; }
  void set_position(int x, int y) {
    this->cfg_x_ = x;
    this->cfg_y_ = y;
  }
  void set_size(int w, int h) {
    this->cfg_w_ = w;
    this->cfg_h_ = h;
  }
  void set_twenty_four_hour(bool on) { this->h24_ = on; }
  void set_hand_width(int px) { this->hand_width_ = px; }
  void set_anti_alias(bool on) { this->anti_alias_ = on; }
  void set_show_face(bool show) { this->show_face_ = show; }
  void set_foreground(Color c) { this->fg_ = c; }
  void set_background(Color c) { this->background_ = c; }
  void set_pointer_color(Color c) {
    this->pointer_ = c;
    this->has_pointer_ = true;
  }
  void set_face_border_color(Color c) {
    this->face_border_ = c;
    this->has_face_border_ = true;
  }
  void set_face_fill_color(Color c) {
    this->face_fill_ = c;
    this->has_face_fill_ = true;
  }

  // --- clockclock24 ---
  void set_transition_length(uint32_t ms) { this->transition_ms_ = ms; }
  void set_spacing(float clocks) { this->spacing_ = clocks; }
  void set_movement(MovementMode m) { this->movement_ = m; }
  void set_mode_speed(float s) { this->mode_speed_ = s; }
  void set_mode(ClockMode m);
  void set_time_mode() { this->set_mode(CC_MODE_TIME); }
  void set_rotate_left_mode() { this->set_mode(CC_MODE_ROTATE_LEFT); }
  void set_flying_birds_mode() { this->set_mode(CC_MODE_FLYING_BIRDS); }
  ClockMode get_mode() const { return this->mode_; }

  // --- analog ---
  void set_analog_seconds(bool on) { this->analog_seconds_ = on; }
  void set_analog_ticks(bool on) { this->analog_ticks_ = on; }
  void set_analog_numerals(bool on) { this->analog_numerals_ = on; }
  void set_analog_font(display::BaseFont *f) { this->analog_font_ = f; }
  void set_second_color(Color c) {
    this->second_ = c;
    this->has_second_ = true;
  }

  // --- digital (7-segment) ---
  void set_digital_seconds(bool on) { this->digital_seconds_ = on; }
  void set_digital_blink(bool on) { this->digital_blink_ = on; }
  void set_digital_blank_leading(bool on) { this->digital_blank_leading_ = on; }
  void set_digital_off_color(Color c) {
    this->digital_off_ = c;
    this->has_digital_off_ = true;
  }

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void draw(display::Display &disp);
  void draw(display::Display &disp, int x, int y, int w, int h);
  void draw(display::Display &disp, int x, int y, int w, int h, Color color);

#ifdef USE_LVGL
  // Draw into an LVGL canvas widget (LVGL 9). Currently the clockclock24 and
  // analog styles are supported on canvas.
  void draw_to_canvas(lv_obj_t *canvas);
#endif

  int min_width() const;
  int min_height() const;

 protected:
  // helpers
  bool now_hms_(int &hh, int &mm, int &ss);  // returns is_valid, applies 12/24h
  Color pointer_color_() const { return this->has_pointer_ ? this->pointer_ : this->fg_; }
  Color face_border_color_() const { return this->has_face_border_ ? this->face_border_ : this->fg_; }
  Color face_fill_color_() const { return this->has_face_fill_ ? this->face_fill_ : this->background_; }
  // Swiss railway second hand is red by default.
  Color second_color_() const { return this->has_second_ ? this->second_ : Color(0xE0, 0x30, 0x30); }
  // Fraction (0..1) of the way through the current second, from millis - lets
  // all three hands sweep continuously instead of jumping.
  float sub_second_(int ss);
  void draw_hand_(display::Display &disp, int cx, int cy, int len, float angle_deg, int width,
                  Color color, Color bg);
  // Thick segment with a solid core and, when anti_alias_ is on, an AA fringe.
  void thick_line_(display::Display &disp, float x1, float y1, float x2, float y2, int width,
                   Color color, Color bg);
  // Xiaolin Wu anti-aliased line, blending `fg` toward `bg` at the edges.
  void aa_line_(display::Display &disp, float x0, float y0, float x1, float y1, Color fg, Color bg);
  static Color blend_(Color bg, Color fg, float a);

  // per-style renderers (display backend)
  void render_clockclock_(display::Display &disp, int x, int y, int w, int h, Color pointer);
  void render_analog_(display::Display &disp, int x, int y, int w, int h, Color pointer);
  void render_digital_(display::Display &disp, int x, int y, int w, int h, Color pointer);
  void render_digital_seg_(display::Display &disp, int x, int y, int w, int h, Color color);

  // shared 7-segment layout (both backends). seg_polys_ returns the active
  // segments as tapered hexagons (6 points each) for the classic pointed-corner
  // look.
  int digital_cells_(int x, int y, int w, int h, DigitalCell out[8]);
  // Fills all 7 segment bounding rects {x,y,w,h} and which are lit for `digit`
  // (0-9, or -2 for a dash). Segments are drawn as rounded-end bars.
  static void seg_rects_(int digit, int dx, int dy, int dw, int dh, int rects[7][4],
                         bool active[7]);
  Color digital_off_color_() const {
    return this->has_digital_off_ ? this->digital_off_ : Color(0x22, 0x22, 0x22);
  }

  // clockclock24 animation engine
  void set_time_(int hh, int mm);
  void retarget_();
  void tick_time_(uint32_t now_ms);
  void tick_rotate_(uint32_t now_ms);
  void tick_birds_(uint32_t now_ms);

#ifdef USE_LVGL
  void canvas_clockclock_(lv_obj_t *canvas, int w, int h);
  void canvas_analog_(lv_obj_t *canvas, int w, int h);
  void canvas_digital_(lv_obj_t *canvas, int w, int h);
  void canvas_hand_(lv_layer_t *layer, lv_draw_line_dsc_t *dsc, int cx, int cy, int len,
                    float angle_deg);
#endif

  // shared config
  time::RealTimeClock *time_{nullptr};
  ClockStyle style_{STYLE_CLOCKCLOCK24};
  int cfg_x_{0};
  int cfg_y_{0};
  int cfg_w_{0};
  int cfg_h_{0};
  bool h24_{true};
  int hand_width_{1};
  bool anti_alias_{false};
  bool show_face_{false};
  Color fg_{Color(255, 255, 255)};
  Color background_{Color(0, 0, 0)};
  Color pointer_{}, face_border_{}, face_fill_{}, second_{};
  bool has_pointer_{false}, has_face_border_{false}, has_face_fill_{false}, has_second_{false};
  bool size_checked_{false};

  // clockclock24
  uint32_t transition_ms_{2000};
  float spacing_{0.6f};
  MovementMode movement_{CC_MOVE_OPPOSITE};
  float mode_speed_{1.0f};
  ClockMode mode_{CC_MODE_TIME};
  int digits_[NUM_DIGITS]{-1, -1, -1, -1};
  int last_key_{-1};
  float cur_[NUM_HANDS];
  float start_[NUM_HANDS];
  float target_[NUM_HANDS];
  bool animating_{false};
  uint32_t anim_start_{0};

  // analog (Swiss railway style)
  bool analog_seconds_{false};
  bool analog_ticks_{true};
  bool analog_numerals_{false};
  display::BaseFont *analog_font_{nullptr};
  int last_sec_{-1};
  uint32_t last_sec_ms_{0};

  // digital (7-segment)
  bool digital_seconds_{false};
  bool digital_blink_{false};
  bool digital_blank_leading_{false};
  Color digital_off_{};
  bool has_digital_off_{false};
};

// Action for clockclock24 idle animations: display_clock.show_time / .rotate_left
// / .flying_birds.
template<typename... Ts> class SetModeAction : public Action<Ts...> {
 public:
  SetModeAction(DisplayClock *parent, ClockMode mode) : parent_(parent), mode_(mode) {}
  void play(Ts... x) override { this->parent_->set_mode(this->mode_); }

 protected:
  DisplayClock *parent_;
  ClockMode mode_;
};

}  // namespace display_clock
}  // namespace esphome
