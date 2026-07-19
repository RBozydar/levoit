#include "clock_clock.h"
#include "esphome/core/log.h"
#include <cmath>
#include <algorithm>

namespace esphome {
namespace clock_clock {

static const char *const TAG = "clock_clock";

static const float PI_F = 3.14159265358979323846f;
static const float PARK = 225.0f;  // idle position: both hands to bottom-left

// Font: for each digit 0-9, the target angle (degrees, 0 = 12 o'clock,
// clockwise) of the two hands of each of its 6 clocks.
// Clock order within a digit: TL, TR, ML, MR, BL, BR (2 columns x 3 rows).
// Angles per clock: {hand_a, hand_b}. This table was tuned by rendering every
// glyph and checking legibility. Because each clock only has two hands a true
// seven-segment "8" is impossible, so "8" keeps a solid box and hangs the mid
// bar off the right vertical (a tiny notch on the left) - still clearly an 8.
static const float FONT[10][CLOCKS_PER_DIGIT][2] = {
    // 0:  TL(90,180) TR(180,270) ML(0,180) MR(0,180) BL(0,90) BR(0,270)
    {{90, 180}, {180, 270}, {0, 180}, {0, 180}, {0, 90}, {0, 270}},
    // 1:  park / down-stub / park / vertical / park / up-stub  (stick on right)
    {{PARK, PARK}, {180, 180}, {PARK, PARK}, {0, 180}, {PARK, PARK}, {0, 0}},
    // 2
    {{90, 90}, {180, 270}, {90, 180}, {0, 270}, {0, 90}, {270, 270}},
    // 3
    {{90, 90}, {180, 270}, {90, 90}, {0, 180}, {90, 90}, {0, 270}},
    // 4
    {{180, 180}, {180, 180}, {0, 90}, {0, 180}, {PARK, PARK}, {0, 0}},
    // 5
    {{90, 180}, {270, 270}, {0, 90}, {180, 270}, {90, 90}, {0, 270}},
    // 6
    {{90, 180}, {270, 270}, {0, 180}, {180, 270}, {0, 90}, {0, 270}},
    // 7
    {{90, 90}, {180, 270}, {PARK, PARK}, {0, 180}, {PARK, PARK}, {0, 0}},
    // 8  - two stacked boxes: ┌┐ / └┘ / └┘  (symmetric, no fake tail)
    {{90, 180}, {180, 270}, {0, 90}, {0, 270}, {0, 90}, {0, 270}},
    // 9  - like the 8 but bottom-left is a lone 3 o'clock hand: ┌┐ / └│ / ╶┘
    {{90, 180}, {180, 270}, {0, 90}, {0, 180}, {90, 90}, {0, 270}},
};

// smootherstep easing (ease-in-out), t in [0,1]
static inline float ease(float t) { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); }

void ClockClock::setup() {
  for (int i = 0; i < NUM_HANDS; i++) {
    this->cur_[i] = PARK;
    this->start_[i] = PARK;
    this->target_[i] = PARK;
  }
  this->animating_ = false;
  this->last_key_ = -1;  // force a sweep on the first valid time
}

void ClockClock::set_time_(int hh, int mm) {
  int lead = hh / 10;
  if (lead == 0 && !this->twenty_four_hour_)
    lead = -1;  // blank the leading zero in 12-hour mode -> those clocks park
  this->digits_[0] = lead;
  this->digits_[1] = hh % 10;
  this->digits_[2] = mm / 10;
  this->digits_[3] = mm % 10;
}

void ClockClock::retarget_() {
  for (int d = 0; d < NUM_DIGITS; d++) {
    int val = this->digits_[d];
    bool blank = (val < 0 || val > 9);
    for (int c = 0; c < CLOCKS_PER_DIGIT; c++) {
      for (int h = 0; h < 2; h++) {
        int hi = (d * CLOCKS_PER_DIGIT + c) * 2 + h;
        float goal = blank ? PARK : FONT[val][c][h];
        float base = fmodf(this->cur_[hi], 360.0f);
        if (base < 0)
          base += 360.0f;
        this->start_[hi] = base;

        float cw = fmodf(goal - base, 360.0f);  // clockwise distance [0,360)
        if (cw < 0)
          cw += 360.0f;
        float ccw = cw - 360.0f;  // anti-clockwise distance (-360,0]

        float delta;
        if (cw < 0.001f) {
          delta = 0.0f;  // already there - never spin a full turn for nothing
        } else {
          switch (this->movement_) {
            case CC_MOVE_CLOCKWISE:
              delta = cw;
              break;
            case CC_MOVE_COUNTER:
              delta = ccw;
              break;
            case CC_MOVE_LONG:
              delta = (cw >= 180.0f) ? cw : ccw;  // always the arc > 180 deg
              break;
            case CC_MOVE_OPPOSITE:
            default:
              // The two hands of a clock counter-rotate: hand 0 clockwise,
              // hand 1 anti-clockwise. This is the signature ClockClock sweep.
              delta = (h == 0) ? cw : ccw;
              break;
          }
        }
        this->target_[hi] = base + delta;
      }
    }
  }
  this->anim_start_ = millis();
  this->animating_ = true;
}

void ClockClock::loop() {
  uint32_t now_ms = millis();
  switch (this->mode_) {
    case CC_MODE_ROTATE_LEFT:
      this->tick_rotate_(now_ms);
      break;
    case CC_MODE_FLYING_BIRDS:
      this->tick_birds_(now_ms);
      break;
    case CC_MODE_TIME:
    default:
      this->tick_time_(now_ms);
      break;
  }
}

void ClockClock::tick_time_(uint32_t now_ms) {
  if (this->time_ != nullptr) {
    ESPTime now = this->time_->now();
    if (now.is_valid()) {
      int hh = now.hour;
      if (!this->twenty_four_hour_) {
        hh = hh % 12;
        if (hh == 0)
          hh = 12;
      }
      int key = hh * 100 + now.minute;
      if (key != this->last_key_) {
        this->last_key_ = key;
        this->set_time_(hh, now.minute);
        this->retarget_();
      }
    }
  }

  if (this->animating_) {
    float t = (this->transition_ms_ == 0)
                  ? 1.0f
                  : (now_ms - this->anim_start_) / (float) this->transition_ms_;
    if (t >= 1.0f) {
      for (int i = 0; i < NUM_HANDS; i++) {
        float f = fmodf(this->target_[i], 360.0f);
        this->cur_[i] = (f < 0) ? f + 360.0f : f;
      }
      this->animating_ = false;
    } else {
      float e = ease(t);
      for (int i = 0; i < NUM_HANDS; i++)
        this->cur_[i] = this->start_[i] + (this->target_[i] - this->start_[i]) * e;
    }
  }
}

void ClockClock::tick_rotate_(uint32_t now_ms) {
  // A full-diameter line spinning anti-clockwise on every clock (a "loading"
  // spinner). 45 deg/s at speed 1.0.
  float ang = fmodf(now_ms / 1000.0f * 45.0f * this->mode_speed_, 360.0f);
  float a = 360.0f - ang;  // anti-clockwise = "rotate left"
  for (int c = 0; c < NUM_CLOCKS; c++) {
    this->cur_[c * 2 + 0] = a;
    this->cur_[c * 2 + 1] = fmodf(a + 180.0f, 360.0f);
  }
}

void ClockClock::tick_birds_(uint32_t now_ms) {
  // Every clock is a seagull silhouette that flaps in sync: the wings sweep from
  // raised (a "V", ~40 deg from vertical) down through level to a drooped "^"
  // (~130 deg), then back up. 0 deg = straight up, 90 = level, >90 = below.
  float ph = now_ms / 1000.0f * 2.0f * this->mode_speed_;
  float wing = 85.0f + 45.0f * sinf(ph);  // half-spread ~40..130 deg
  float left = 360.0f - wing;
  for (int c = 0; c < NUM_CLOCKS; c++) {
    this->cur_[c * 2 + 0] = wing;   // right wing
    this->cur_[c * 2 + 1] = left;   // left wing (mirror)
  }
}

void ClockClock::set_mode(ClockClockMode m) {
  if (m == this->mode_)
    return;
  this->mode_ = m;
  if (m == CC_MODE_TIME) {
    // Sweep from wherever the hands are now into the current time.
    this->last_key_ = -1;
    this->animating_ = false;
  }
}

void ClockClock::draw_hand_(display::Display &disp, int cx, int cy, int len, float angle_deg,
                            Color color) {
  float rad = angle_deg * PI_F / 180.0f;
  float dx = sinf(rad);   // 0 deg -> up
  float dy = -cosf(rad);
  int ex = cx + (int) lroundf(dx * len);
  int ey = cy + (int) lroundf(dy * len);

  // Thicken by drawing parallel lines offset along the perpendicular. Use
  // *integer* offsets (0, then +/-1, +/-2 ...) so adjacent lines touch. Half
  // pixel offsets would round away from zero and leave a 1px gap in the middle,
  // making a single hand look like two.
  float px = -dy, py = dx;
  int half = (this->hand_width_ - 1) / 2;
  for (int i = 0; i < this->hand_width_; i++) {
    int off = i - half;
    int ox = (int) lroundf(px * off);
    int oy = (int) lroundf(py * off);
    disp.line(cx + ox, cy + oy, ex + ox, ey + oy, color);
  }
}

void ClockClock::draw(display::Display &disp) {
  int x = this->cfg_x_;
  int y = this->cfg_y_;
  int w = (this->cfg_w_ > 0) ? this->cfg_w_ : (disp.get_width() - x);
  int h = (this->cfg_h_ > 0) ? this->cfg_h_ : (disp.get_height() - y);
  this->draw(disp, x, y, w, h);
}

void ClockClock::draw(display::Display &disp, int x, int y, int w, int h) {
  this->render_(disp, x, y, w, h, this->pointer_color_());
}

void ClockClock::draw(display::Display &disp, int x, int y, int w, int h, Color color) {
  // Explicit colour overrides the configured pointer colour for this call.
  this->render_(disp, x, y, w, h, color);
}

void ClockClock::render_(display::Display &disp, int x, int y, int w, int h, Color pointer) {
  // Once, compare the real draw area to the recommended minimum and log it.
  if (!this->size_checked_) {
    this->size_checked_ = true;
    int mw = this->min_width(), mh = this->min_height();
    if (w < mw || h < mh) {
      ESP_LOGW(TAG, "Draw area %dx%d px is below the recommended minimum %dx%d - digits may be hard to read",
               w, h, mw, mh);
    } else {
      ESP_LOGI(TAG, "Draw area %dx%d px (recommended min %dx%d) - OK", w, h, mw, mh);
    }
  }

  // Fill the area around the clocks (default black; lets colour panels have a
  // real backdrop). On mono this is COLOR_OFF and just clears the region.
  disp.filled_rectangle(x, y, w, h, this->background_);

  // The grid is 8 clocks wide (+ a gap between HH and MM) and 3 tall.
  float cols = 8.0f + this->spacing_;
  float rows = 3.0f;
  float cell = std::min(w / cols, h / rows);
  if (cell < 2.0f)
    return;  // too small to draw anything meaningful

  int radius = (int) (cell / 2.0f);
  int len = (int) (radius * 0.86f);  // hands slightly shorter than the face

  float grid_w = cell * cols;
  float grid_h = cell * rows;
  float ox = x + (w - grid_w) / 2.0f;  // centre the grid in the rectangle
  float oy = y + (h - grid_h) / 2.0f;

  Color face = this->clock_bg_color_();
  Color border = this->border_color_();

  for (int c = 0; c < NUM_CLOCKS; c++) {
    int digit = c / CLOCKS_PER_DIGIT;
    int cell_i = c % CLOCKS_PER_DIGIT;
    int col = cell_i % 2;  // 0 (left) or 1 (right) within the digit
    int row = cell_i / 2;  // 0..2

    float gcol = digit * 2 + col + (digit >= 2 ? this->spacing_ : 0.0f);
    int ccx = (int) lroundf(ox + (gcol + 0.5f) * cell);
    int ccy = (int) lroundf(oy + (row + 0.5f) * cell);

    if (this->show_face_ && radius > 3) {
      disp.filled_circle(ccx, ccy, radius - 1, face);  // clock face fill
      disp.circle(ccx, ccy, radius - 1, border);       // clock rim
    }

    float a0 = this->cur_[c * 2 + 0];
    float a1 = this->cur_[c * 2 + 1];
    this->draw_hand_(disp, ccx, ccy, len, a0, pointer);
    // Both hands are the same length, so when they point the same way they fully
    // overlap - draw the second one only if it differs (mod 360).
    float delta = fmodf(fabsf(a1 - a0), 360.0f);
    if (delta > 0.5f && delta < 359.5f)
      this->draw_hand_(disp, ccx, ccy, len, a1, pointer);
  }
}

int ClockClock::min_width() const {
  // ~7px hands need a ~16px cell to read; 8 clocks + gap.
  return (int) std::ceil(16.0f * (8.0f + this->spacing_));
}
int ClockClock::min_height() const { return 16 * 3; }

void ClockClock::dump_config() {
  static const char *const MOVES[] = {"opposite", "clockwise", "counter", "long"};
  ESP_LOGCONFIG(TAG, "ClockClock:");
  ESP_LOGCONFIG(TAG, "  Transition: %u ms", this->transition_ms_);
  ESP_LOGCONFIG(TAG, "  Movement: %s", MOVES[this->movement_]);
  ESP_LOGCONFIG(TAG, "  Hand width: %d px", this->hand_width_);
  ESP_LOGCONFIG(TAG, "  Spacing: %.2f clocks", this->spacing_);
  ESP_LOGCONFIG(TAG, "  24-hour: %s", YESNO(this->twenty_four_hour_));
  ESP_LOGCONFIG(TAG, "  Area: x=%d y=%d w=%d h=%d (0 = auto / fill to edge)", this->cfg_x_,
                this->cfg_y_, this->cfg_w_, this->cfg_h_);
  ESP_LOGCONFIG(TAG, "  Recommended min resolution: %dx%d px", this->min_width(),
                this->min_height());
}

#ifdef USE_LVGL
void ClockClock::draw_to_canvas(lv_obj_t *canvas) {
  if (canvas == nullptr)
    return;
  int w = lv_obj_get_width(canvas);
  int h = lv_obj_get_height(canvas);
  if (w <= 0 || h <= 0)
    return;

  auto to_lv = [](Color c) { return lv_color_make(c.r, c.g, c.b); };

  // Background around the clocks.
  lv_canvas_fill_bg(canvas, to_lv(this->background_), LV_OPA_COVER);

  float cols = 8.0f + this->spacing_;
  float rows = 3.0f;
  float cell = std::min(w / cols, h / rows);
  if (cell < 2.0f) {
    lv_obj_invalidate(canvas);
    return;
  }
  int radius = (int) (cell / 2.0f);
  int len = (int) (radius * 0.86f);
  float grid_w = cell * cols, grid_h = cell * rows;
  float ox = (w - grid_w) / 2.0f, oy = (h - grid_h) / 2.0f;

  lv_layer_t layer;
  lv_canvas_init_layer(canvas, &layer);

  lv_draw_line_dsc_t hand;
  lv_draw_line_dsc_init(&hand);
  hand.color = to_lv(this->pointer_color_());
  hand.width = this->hand_width_;
  hand.round_start = hand.round_end = true;

  bool faces = this->show_face_ && radius > 3;
  lv_draw_rect_dsc_t face;
  if (faces) {
    lv_draw_rect_dsc_init(&face);
    face.radius = LV_RADIUS_CIRCLE;
    face.bg_color = to_lv(this->clock_bg_color_());
    face.bg_opa = LV_OPA_COVER;
    face.border_color = to_lv(this->border_color_());
    face.border_width = 1;
    face.border_opa = LV_OPA_COVER;
  }

  for (int c = 0; c < NUM_CLOCKS; c++) {
    int digit = c / CLOCKS_PER_DIGIT;
    int cell_i = c % CLOCKS_PER_DIGIT;
    int col = cell_i % 2;
    int row = cell_i / 2;
    float gcol = digit * 2 + col + (digit >= 2 ? this->spacing_ : 0.0f);
    int ccx = (int) lroundf(ox + (gcol + 0.5f) * cell);
    int ccy = (int) lroundf(oy + (row + 0.5f) * cell);

    if (faces) {
      lv_area_t area = {ccx - radius + 1, ccy - radius + 1, ccx + radius - 1, ccy + radius - 1};
      lv_draw_rect(&layer, &face, &area);
    }

    float a0 = this->cur_[c * 2 + 0];
    float a1 = this->cur_[c * 2 + 1];
    this->canvas_hand_(&layer, &hand, ccx, ccy, len, a0);
    float delta = fmodf(fabsf(a1 - a0), 360.0f);
    if (delta > 0.5f && delta < 359.5f)
      this->canvas_hand_(&layer, &hand, ccx, ccy, len, a1);
  }

  lv_canvas_finish_layer(canvas, &layer);
  lv_obj_invalidate(canvas);
}

void ClockClock::canvas_hand_(lv_layer_t *layer, lv_draw_line_dsc_t *dsc, int cx, int cy, int len,
                              float angle_deg) {
  float rad = angle_deg * PI_F / 180.0f;
  int ex = cx + (int) lroundf(sinf(rad) * len);
  int ey = cy - (int) lroundf(cosf(rad) * len);
  dsc->p1.x = (lv_value_precise_t) cx;
  dsc->p1.y = (lv_value_precise_t) cy;
  dsc->p2.x = (lv_value_precise_t) ex;
  dsc->p2.y = (lv_value_precise_t) ey;
  lv_draw_line(layer, dsc);
}
#endif  // USE_LVGL

}  // namespace clock_clock
}  // namespace esphome
