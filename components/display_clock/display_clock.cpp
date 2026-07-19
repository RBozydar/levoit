#include "display_clock.h"
#include "esphome/core/log.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>

namespace esphome {
namespace display_clock {

static const char *const TAG = "display_clock";

static const float PI_F = 3.14159265358979323846f;
static const float PARK = 225.0f;  // clockclock idle: both hands to bottom-left

// clockclock24 font: target angle (deg, 0 = 12 o'clock, clockwise) of the two
// hands of each of a digit's 6 clocks (order TL, TR, ML, MR, BL, BR).
static const float FONT[10][CLOCKS_PER_DIGIT][2] = {
    {{90, 180}, {180, 270}, {0, 180}, {0, 180}, {0, 90}, {0, 270}},           // 0
    {{PARK, PARK}, {180, 180}, {PARK, PARK}, {0, 180}, {PARK, PARK}, {0, 0}},  // 1
    {{90, 90}, {180, 270}, {90, 180}, {0, 270}, {0, 90}, {270, 270}},         // 2
    {{90, 90}, {180, 270}, {90, 90}, {0, 180}, {90, 90}, {0, 270}},           // 3
    {{180, 180}, {180, 180}, {0, 90}, {0, 180}, {PARK, PARK}, {0, 0}},        // 4
    {{90, 180}, {270, 270}, {0, 90}, {180, 270}, {90, 90}, {0, 270}},         // 5
    {{90, 180}, {270, 270}, {0, 180}, {180, 270}, {0, 90}, {0, 270}},         // 6
    {{90, 90}, {180, 270}, {PARK, PARK}, {0, 180}, {PARK, PARK}, {0, 0}},     // 7
    {{90, 180}, {180, 270}, {0, 90}, {0, 270}, {0, 90}, {0, 270}},            // 8 (two-box)
    {{90, 180}, {180, 270}, {0, 90}, {0, 180}, {90, 90}, {0, 270}},           // 9
};

static inline float ease(float t) { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); }

void DisplayClock::setup() {
  for (int i = 0; i < NUM_HANDS; i++) {
    this->cur_[i] = PARK;
    this->start_[i] = PARK;
    this->target_[i] = PARK;
  }
  this->animating_ = false;
  this->last_key_ = -1;
}

bool DisplayClock::now_hms_(int &hh, int &mm, int &ss) {
  if (this->time_ == nullptr)
    return false;
  ESPTime t = this->time_->now();
  if (!t.is_valid())
    return false;
  hh = t.hour;
  mm = t.minute;
  ss = t.second;
  if (!this->h24_) {
    hh = hh % 12;
    if (hh == 0)
      hh = 12;
  }
  return true;
}

// ---------------------------------------------------------------------------
// clockclock24 animation engine
// ---------------------------------------------------------------------------
void DisplayClock::set_time_(int hh, int mm) {
  int lead = hh / 10;
  if (lead == 0 && !this->h24_)
    lead = -1;  // blank leading zero in 12h mode
  this->digits_[0] = lead;
  this->digits_[1] = hh % 10;
  this->digits_[2] = mm / 10;
  this->digits_[3] = mm % 10;
}

void DisplayClock::retarget_() {
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
        float cw = fmodf(goal - base, 360.0f);
        if (cw < 0)
          cw += 360.0f;
        float ccw = cw - 360.0f;
        float delta;
        if (cw < 0.001f) {
          delta = 0.0f;
        } else {
          switch (this->movement_) {
            case CC_MOVE_CLOCKWISE:
              delta = cw;
              break;
            case CC_MOVE_COUNTER:
              delta = ccw;
              break;
            case CC_MOVE_LONG:
              delta = (cw >= 180.0f) ? cw : ccw;
              break;
            case CC_MOVE_OPPOSITE:
            default:
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

void DisplayClock::tick_time_(uint32_t now_ms) {
  int hh, mm, ss;
  if (this->now_hms_(hh, mm, ss)) {
    int key = hh * 100 + mm;
    if (key != this->last_key_) {
      this->last_key_ = key;
      this->set_time_(hh, mm);
      this->retarget_();
    }
  }
  if (this->animating_) {
    float t = (this->transition_ms_ == 0) ? 1.0f
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

void DisplayClock::tick_rotate_(uint32_t now_ms) {
  float ang = fmodf(now_ms / 1000.0f * 45.0f * this->mode_speed_, 360.0f);
  float a = 360.0f - ang;
  for (int c = 0; c < NUM_CLOCKS; c++) {
    this->cur_[c * 2 + 0] = a;
    this->cur_[c * 2 + 1] = fmodf(a + 180.0f, 360.0f);
  }
}

void DisplayClock::tick_birds_(uint32_t now_ms) {
  float ph = now_ms / 1000.0f * 2.0f * this->mode_speed_;
  float wing = 85.0f + 45.0f * sinf(ph);  // sweeps ~40..130 deg
  float left = 360.0f - wing;
  for (int c = 0; c < NUM_CLOCKS; c++) {
    this->cur_[c * 2 + 0] = wing;
    this->cur_[c * 2 + 1] = left;
  }
}

void DisplayClock::set_mode(ClockMode m) {
  if (m == this->mode_)
    return;
  this->mode_ = m;
  if (m == CC_MODE_TIME) {
    this->last_key_ = -1;
    this->animating_ = false;
  }
}

void DisplayClock::loop() {
  if (this->style_ != STYLE_CLOCKCLOCK24)
    return;  // analog / digital compute straight from now() at draw time
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

// ---------------------------------------------------------------------------
// drawing (display backend)
// ---------------------------------------------------------------------------
Color DisplayClock::blend_(Color bg, Color fg, float a) {
  if (a <= 0.0f)
    return bg;
  if (a >= 1.0f)
    return fg;
  uint8_t r = bg.r + (int) lroundf((fg.r - bg.r) * a);
  uint8_t g = bg.g + (int) lroundf((fg.g - bg.g) * a);
  uint8_t b = bg.b + (int) lroundf((fg.b - bg.b) * a);
  return Color(r, g, b);
}

// Xiaolin Wu's line algorithm - plots edge pixels with fractional coverage.
void DisplayClock::aa_line_(display::Display &disp, float x0, float y0, float x1, float y1, Color fg,
                            Color bg) {
  auto ipart = [](float x) { return floorf(x); };
  auto fpart = [](float x) { return x - floorf(x); };
  auto rfpart = [&](float x) { return 1.0f - fpart(x); };
  auto plot = [&](int x, int y, float c) {
    if (c > 0.02f)
      disp.draw_pixel_at(x, y, blend_(bg, fg, c));
  };
  bool steep = fabsf(y1 - y0) > fabsf(x1 - x0);
  if (steep) {
    std::swap(x0, y0);
    std::swap(x1, y1);
  }
  if (x0 > x1) {
    std::swap(x0, x1);
    std::swap(y0, y1);
  }
  float dx = x1 - x0, dy = y1 - y0;
  float grad = (dx == 0.0f) ? 1.0f : dy / dx;
  float intery = y0 + grad * (roundf(x0) - x0) + grad;
  int xpxl1 = (int) roundf(x0);
  int xpxl2 = (int) roundf(x1);
  // endpoints (approximate - the hands are long, endpoints barely matter)
  for (int x = xpxl1; x <= xpxl2; x++) {
    float yy = (x == xpxl1) ? (y0 + grad * (x - x0)) : intery;
    int yl = (int) ipart(yy);
    if (steep) {
      plot(yl, x, rfpart(yy));
      plot(yl + 1, x, fpart(yy));
    } else {
      plot(x, yl, rfpart(yy));
      plot(x, yl + 1, fpart(yy));
    }
    if (x != xpxl1)
      intery += grad;
  }
}

void DisplayClock::thick_line_(display::Display &disp, float x1, float y1, float x2, float y2,
                               int width, Color color, Color bg) {
  float dx = x2 - x1, dy = y2 - y1;
  float len = sqrtf(dx * dx + dy * dy);
  float px = (len < 0.001f) ? 0.0f : -dy / len;  // perpendicular unit
  float py = (len < 0.001f) ? 0.0f : dx / len;
  int half = (width - 1) / 2;
  int ix1 = (int) lroundf(x1), iy1 = (int) lroundf(y1);
  int ix2 = (int) lroundf(x2), iy2 = (int) lroundf(y2);
  // solid core
  for (int i = 0; i < width; i++) {
    int off = i - half;
    int ox = (int) lroundf(px * off), oy = (int) lroundf(py * off);
    disp.line(ix1 + ox, iy1 + oy, ix2 + ox, iy2 + oy, color);
  }
  // anti-aliased fringe just outside each long edge
  if (this->anti_alias_) {
    float edges[2] = {-half - 0.5f, (width - 1 - half) + 0.5f};
    for (float off : edges)
      this->aa_line_(disp, x1 + px * off, y1 + py * off, x2 + px * off, y2 + py * off, color, bg);
  }
}

void DisplayClock::draw_hand_(display::Display &disp, int cx, int cy, int len, float angle_deg,
                              int width, Color color, Color bg) {
  float rad = angle_deg * PI_F / 180.0f;
  float exf = cx + sinf(rad) * len, eyf = cy - cosf(rad) * len;
  this->thick_line_(disp, cx, cy, exf, eyf, width, color, bg);
}

void DisplayClock::draw(display::Display &disp) {
  int x = this->cfg_x_;
  int y = this->cfg_y_;
  int w = (this->cfg_w_ > 0) ? this->cfg_w_ : (disp.get_width() - x);
  int h = (this->cfg_h_ > 0) ? this->cfg_h_ : (disp.get_height() - y);
  this->draw(disp, x, y, w, h);
}

void DisplayClock::draw(display::Display &disp, int x, int y, int w, int h) {
  this->draw(disp, x, y, w, h, this->pointer_color_());
}

void DisplayClock::draw(display::Display &disp, int x, int y, int w, int h, Color color) {
  if (!this->size_checked_) {
    this->size_checked_ = true;
    int mw = this->min_width(), mh = this->min_height();
    if (w < mw || h < mh)
      ESP_LOGW(TAG, "Draw area %dx%d px is below the recommended minimum %dx%d", w, h, mw, mh);
    else
      ESP_LOGI(TAG, "Draw area %dx%d px (min %dx%d) - OK", w, h, mw, mh);
  }
  switch (this->style_) {
    case STYLE_ANALOG:
      this->render_analog_(disp, x, y, w, h, color);
      break;
    case STYLE_DIGITAL:
      this->render_digital_(disp, x, y, w, h, color);
      break;
    case STYLE_CLOCKCLOCK24:
    default:
      this->render_clockclock_(disp, x, y, w, h, color);
      break;
  }
}

void DisplayClock::render_clockclock_(display::Display &disp, int x, int y, int w, int h,
                                      Color pointer) {
  disp.filled_rectangle(x, y, w, h, this->background_);
  float cols = 8.0f + this->spacing_;
  float rows = 3.0f;
  // Odd cell/diameter -> every clock has a true centre pixel, so the hands
  // pivot cleanly (an even diameter puts the centre between pixels and the
  // spinning hands jump). Drop to the next odd down; it always still fits.
  int cell = (int) std::min((float) w / cols, (float) h / rows);
  if ((cell & 1) == 0)
    cell -= 1;
  if (cell < 3)
    return;
  int radius = cell / 2;  // (cell-1)/2; drawn face diameter is 2*radius+1
  int len = (int) (radius * 0.86f);
  float grid_w = cell * cols, grid_h = cell * rows;
  int ox = x + (int) lroundf((w - grid_w) / 2.0f);
  int oy = y + (int) lroundf((h - grid_h) / 2.0f);
  Color face = this->face_fill_color_();
  Color border = this->face_border_color_();
  for (int c = 0; c < NUM_CLOCKS; c++) {
    int digit = c / CLOCKS_PER_DIGIT;
    int cell_i = c % CLOCKS_PER_DIGIT;
    int col = cell_i % 2;
    int row = cell_i / 2;
    float gcol = digit * 2 + col + (digit >= 2 ? this->spacing_ : 0.0f);
    int ccx = ox + (int) lroundf(gcol * cell) + radius;  // integer centre
    int ccy = oy + row * cell + radius;
    if (this->show_face_ && radius > 3) {
      disp.filled_circle(ccx, ccy, radius - 1, face);
      disp.circle(ccx, ccy, radius - 1, border);
    }
    float a0 = this->cur_[c * 2 + 0];
    float a1 = this->cur_[c * 2 + 1];
    Color hand_bg = this->show_face_ ? face : this->background_;
    this->draw_hand_(disp, ccx, ccy, len, a0, this->hand_width_, pointer, hand_bg);
    float delta = fmodf(fabsf(a1 - a0), 360.0f);
    if (delta > 0.5f && delta < 359.5f)
      this->draw_hand_(disp, ccx, ccy, len, a1, this->hand_width_, pointer, hand_bg);
  }
}

float DisplayClock::sub_second_(int ss) {
  uint32_t now = millis();
  if (ss != this->last_sec_) {
    this->last_sec_ = ss;
    this->last_sec_ms_ = now;
  }
  float frac = (now - this->last_sec_ms_) / 1000.0f;
  return (frac > 1.0f) ? 1.0f : frac;
}

void DisplayClock::render_analog_(display::Display &disp, int x, int y, int w, int h, Color pointer) {
  // Swiss railway (SBB) style: bold bar markers, baton hands, red paddle second.
  disp.filled_rectangle(x, y, w, h, this->background_);
  int cx = x + w / 2, cy = y + h / 2;
  int R = std::min(w, h) / 2 - 1;
  if (R < 6)
    return;
  Color border = this->face_border_color_();
  if (this->show_face_) {
    disp.filled_circle(cx, cy, R, this->face_fill_color_());
    disp.circle(cx, cy, R, border);
  }
  if (this->analog_ticks_) {
    int hour_w = std::max(2, R / 18);
    Color tbg = this->show_face_ ? this->face_fill_color_() : this->background_;
    for (int i = 0; i < 60; i++) {
      bool hour = (i % 5 == 0);
      float a = i * 6.0f * PI_F / 180.0f;
      float inner = hour ? 0.74f : 0.89f;
      float x1 = cx + sinf(a) * R * inner;
      float y1 = cy - cosf(a) * R * inner;
      float x2 = cx + sinf(a) * R * 0.96f;
      float y2 = cy - cosf(a) * R * 0.96f;
      this->thick_line_(disp, x1, y1, x2, y2, hour ? hour_w : 1, border, tbg);
    }
  }
  if (this->analog_numerals_ && this->analog_font_ != nullptr) {
    for (int n = 1; n <= 12; n++) {
      float a = n * 30.0f * PI_F / 180.0f;
      int nx = cx + (int) lroundf(sinf(a) * R * 0.62f);
      int ny = cy - (int) lroundf(cosf(a) * R * 0.62f);
      disp.printf(nx, ny, this->analog_font_, pointer, display::TextAlign::CENTER, "%d", n);
    }
  }
  int hh, mm, ss;
  if (this->now_hms_(hh, mm, ss)) {
    // Continuous sweep: every hand glides, none jump or pause.
    float cs = ss + this->sub_second_(ss);  // 0..60
    float cm = mm + cs / 60.0f;             // continuous minutes
    float ch = (hh % 12) + cm / 60.0f;      // continuous hours
    float hour_a = ch * 30.0f;
    float min_a = cm * 6.0f;
    Color abg = this->show_face_ ? this->face_fill_color_() : this->background_;
    this->draw_hand_(disp, cx, cy, (int) (R * 0.55f), hour_a, std::max(this->hand_width_ + 2, R / 12),
                     pointer, abg);
    this->draw_hand_(disp, cx, cy, (int) (R * 0.82f), min_a, std::max(this->hand_width_ + 1, R / 16),
                     pointer, abg);
    if (this->analog_seconds_) {
      float sec_a = cs * 6.0f;
      Color sc = this->second_color_();
      this->draw_hand_(disp, cx, cy, (int) (R * 0.90f), sec_a, 1, sc, abg);
      // the paddle / lollipop about two-thirds out
      float ar = sec_a * PI_F / 180.0f;
      int px = cx + (int) lroundf(sinf(ar) * R * 0.62f);
      int py = cy - (int) lroundf(cosf(ar) * R * 0.62f);
      disp.filled_circle(px, py, std::max(2, R / 11), sc);
    }
  }
  disp.filled_circle(cx, cy, std::max(2, R / 14), pointer);
}

void DisplayClock::render_digital_(display::Display &disp, int x, int y, int w, int h, Color pointer) {
  disp.filled_rectangle(x, y, w, h, this->background_);
  this->render_digital_seg_(disp, x, y, w, h, pointer);  // self-contained 7-segment
}

// segment on/off bitmask per digit (a=1,b=2,c=4,d=8,e=16,f=32,g=64)
static const uint8_t SEG7[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};

void DisplayClock::seg_rects_(int digit, int dx, int dy, int dw, int dh, int rects[7][4],
                              bool active[7]) {
  int t = std::max(2, dw / 6);
  int vh = (dh - 3 * t) / 2;
  if (vh < t)
    vh = t;
  int xr = dx + dw - t;
  int y1 = dy + t + vh, y2 = dy + 2 * t + vh, yd = dy + 2 * t + 2 * vh;
  int seg[7][4] = {  // {isHorizontal, x, y, len} for a,b,c,d,e,f,g
      {1, dx + t, dy, dw - 2 * t}, {0, xr, dy + t, vh},          {0, xr, y2, vh},
      {1, dx + t, yd, dw - 2 * t}, {0, dx, y2, vh},              {0, dx, dy + t, vh},
      {1, dx + t, y1, dw - 2 * t},
  };
  int mask = (digit >= 0 && digit <= 9) ? SEG7[digit] : (digit == -2 ? (1 << 6) : 0);
  for (int i = 0; i < 7; i++) {
    active[i] = mask & (1 << i);
    int hz = seg[i][0];
    rects[i][0] = seg[i][1];
    rects[i][1] = seg[i][2];
    rects[i][2] = hz ? seg[i][3] : t;  // w
    rects[i][3] = hz ? t : seg[i][3];  // h
  }
}

// A rounded-end bar (capsule): core rectangle + a circle at each end.
static void capsule_disp_(display::Display &disp, int x, int y, int w, int h, Color col) {
  int r = std::min(w, h) / 2;
  if (r < 1) {
    disp.filled_rectangle(x, y, w, h, col);
    return;
  }
  if (w >= h) {  // horizontal
    disp.filled_rectangle(x + r, y, w - 2 * r, h, col);
    disp.filled_circle(x + r, y + r, r, col);
    disp.filled_circle(x + w - r - 1, y + r, r, col);
  } else {  // vertical
    disp.filled_rectangle(x, y + r, w, h - 2 * r, col);
    disp.filled_circle(x + r, y + r, r, col);
    disp.filled_circle(x + r, y + h - r - 1, r, col);
  }
}

int DisplayClock::digital_cells_(int x, int y, int w, int h, DigitalCell out[8]) {
  x += 1;
  w -= 2;  // small margin so the rounded ends never clip at the edges
  int hh, mm, ss;
  bool valid = this->now_hms_(hh, mm, ss);
  bool colon_on = !(this->digital_blink_ && ((millis() / 1000) & 1));
  int colon_val = colon_on ? 10 : 11;
  int vals[8];
  int nv = 0;
  if (!valid) {
    vals[nv++] = -2;
    vals[nv++] = -2;
    vals[nv++] = colon_val;
    vals[nv++] = -2;
    vals[nv++] = -2;
    if (this->digital_seconds_) {
      vals[nv++] = colon_val;
      vals[nv++] = -2;
      vals[nv++] = -2;
    }
  } else {
    vals[nv++] = (this->digital_blank_leading_ && hh < 10) ? -1 : hh / 10;
    vals[nv++] = hh % 10;
    vals[nv++] = colon_val;
    vals[nv++] = mm / 10;
    vals[nv++] = mm % 10;
    if (this->digital_seconds_) {
      vals[nv++] = colon_val;
      vals[nv++] = ss / 10;
      vals[nv++] = ss % 10;
    }
  }
  const float GAP = 0.18f, COLON_W = 0.45f;
  auto uw = [&](int v) { return (v == 10 || v == 11) ? COLON_W : 1.0f; };
  float units = (nv - 1) * GAP;
  for (int i = 0; i < nv; i++)
    units += uw(vals[i]);
  float dwf = w / units;                                       // one digit width
  int dh = (int) std::min((float) h, dwf * 1.9f);
  int dy = y + (h - dh) / 2;
  float total = (nv - 1) * GAP * dwf;
  for (int i = 0; i < nv; i++)
    total += uw(vals[i]) * dwf;
  float cur = x + (w - total) / 2.0f;
  for (int i = 0; i < nv; i++) {
    float cwf = uw(vals[i]) * dwf;
    out[i] = {vals[i], (int) lroundf(cur), dy, (int) lroundf(cwf), dh};
    cur += cwf + GAP * dwf;
  }
  return nv;
}

void DisplayClock::render_digital_seg_(display::Display &disp, int x, int y, int w, int h,
                                       Color color) {
  DigitalCell cells[8];
  int n = this->digital_cells_(x, y, w, h, cells);
  Color off = this->digital_off_color_();
  for (int i = 0; i < n; i++) {
    DigitalCell &c = cells[i];
    if (c.val == 10 || c.val == 11) {  // colon: on = foreground, blinked off = ghost
      Color cc = (c.val == 10) ? color : off;
      int r = std::max(1, c.w / 3);
      disp.filled_circle(c.x + c.w / 2, c.y + c.h / 3, r, cc);
      disp.filled_circle(c.x + c.w / 2, c.y + 2 * c.h / 3, r, cc);
    } else if (c.val >= 0 || c.val == -2) {  // digit / dash  (-1 blank => nothing)
      int rects[7][4];
      bool act[7];
      this->seg_rects_(c.val, c.x, c.y, c.w, c.h, rects, act);
      for (int sg = 0; sg < 7; sg++)
        capsule_disp_(disp, rects[sg][0], rects[sg][1], rects[sg][2], rects[sg][3],
                      act[sg] ? color : off);
    }
  }
}

int DisplayClock::min_width() const {
  switch (this->style_) {
    case STYLE_ANALOG:
      return 24;
    case STYLE_DIGITAL:
      return 24;  // really font-dependent
    case STYLE_CLOCKCLOCK24:
    default:
      return (int) std::ceil(16.0f * (8.0f + this->spacing_));
  }
}
int DisplayClock::min_height() const {
  switch (this->style_) {
    case STYLE_ANALOG:
      return 24;
    case STYLE_DIGITAL:
      return 12;
    case STYLE_CLOCKCLOCK24:
    default:
      return 48;
  }
}

void DisplayClock::dump_config() {
  static const char *const STYLES[] = {"clockclock24", "analog", "digital"};
  static const char *const MOVES[] = {"opposite", "clockwise", "counter", "long"};
  ESP_LOGCONFIG(TAG, "DisplayClock:");
  ESP_LOGCONFIG(TAG, "  Style: %s", STYLES[this->style_]);
  ESP_LOGCONFIG(TAG, "  24-hour: %s", YESNO(this->h24_));
  ESP_LOGCONFIG(TAG, "  Area: x=%d y=%d w=%d h=%d (0 = auto)", this->cfg_x_, this->cfg_y_,
                this->cfg_w_, this->cfg_h_);
  if (this->style_ == STYLE_CLOCKCLOCK24) {
    ESP_LOGCONFIG(TAG, "  Transition: %u ms, movement: %s", this->transition_ms_,
                  MOVES[this->movement_]);
  }
  ESP_LOGCONFIG(TAG, "  Recommended min: %dx%d px", this->min_width(), this->min_height());
}

// ---------------------------------------------------------------------------
// LVGL canvas backend (LVGL 9)
// ---------------------------------------------------------------------------
#ifdef USE_LVGL
void DisplayClock::draw_to_canvas(lv_obj_t *canvas) {
  if (canvas == nullptr)
    return;
  int w = lv_obj_get_width(canvas);
  int h = lv_obj_get_height(canvas);
  if (w <= 0 || h <= 0)
    return;
  switch (this->style_) {
    case STYLE_ANALOG:
      this->canvas_analog_(canvas, w, h);
      break;
    case STYLE_DIGITAL:
      this->canvas_digital_(canvas, w, h);
      break;
    case STYLE_CLOCKCLOCK24:
    default:
      this->canvas_clockclock_(canvas, w, h);
      break;
  }
  lv_obj_invalidate(canvas);
}

void DisplayClock::canvas_hand_(lv_layer_t *layer, lv_draw_line_dsc_t *dsc, int cx, int cy,
                                int len, float angle_deg) {
  float rad = angle_deg * PI_F / 180.0f;
  int ex = cx + (int) lroundf(sinf(rad) * len);
  int ey = cy - (int) lroundf(cosf(rad) * len);
  dsc->p1.x = (lv_value_precise_t) cx;
  dsc->p1.y = (lv_value_precise_t) cy;
  dsc->p2.x = (lv_value_precise_t) ex;
  dsc->p2.y = (lv_value_precise_t) ey;
  lv_draw_line(layer, dsc);
}

void DisplayClock::canvas_clockclock_(lv_obj_t *canvas, int w, int h) {
  auto to_lv = [](Color c) { return lv_color_make(c.r, c.g, c.b); };
  lv_canvas_fill_bg(canvas, to_lv(this->background_), LV_OPA_COVER);
  float cols = 8.0f + this->spacing_;
  float rows = 3.0f;
  // Odd diameter -> true centre pixel per clock (even => hands jump when spinning).
  int cell = (int) std::min((float) w / cols, (float) h / rows);
  if ((cell & 1) == 0)
    cell -= 1;
  if (cell < 3)
    return;
  int radius = cell / 2;
  int len = (int) (radius * 0.86f);
  float grid_w = cell * cols, grid_h = cell * rows;
  int ox = (int) lroundf((w - grid_w) / 2.0f), oy = (int) lroundf((h - grid_h) / 2.0f);

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
    face.bg_color = to_lv(this->face_fill_color_());
    face.bg_opa = LV_OPA_COVER;
    face.border_color = to_lv(this->face_border_color_());
    face.border_width = 1;
    face.border_opa = LV_OPA_COVER;
  }
  for (int c = 0; c < NUM_CLOCKS; c++) {
    int digit = c / CLOCKS_PER_DIGIT;
    int cell_i = c % CLOCKS_PER_DIGIT;
    int col = cell_i % 2, row = cell_i / 2;
    float gcol = digit * 2 + col + (digit >= 2 ? this->spacing_ : 0.0f);
    int ccx = ox + (int) lroundf(gcol * cell) + radius;  // integer centre
    int ccy = oy + row * cell + radius;
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
}

void DisplayClock::canvas_analog_(lv_obj_t *canvas, int w, int h) {
  auto to_lv = [](Color c) { return lv_color_make(c.r, c.g, c.b); };
  lv_canvas_fill_bg(canvas, to_lv(this->background_), LV_OPA_COVER);
  int cx = w / 2, cy = h / 2;
  int R = std::min(w, h) / 2 - 1;
  if (R < 6)
    return;

  lv_layer_t layer;
  lv_canvas_init_layer(canvas, &layer);
  lv_draw_line_dsc_t ln;
  lv_draw_line_dsc_init(&ln);
  ln.color = to_lv(this->face_border_color_());
  ln.round_start = ln.round_end = true;

  // Filled dial - without it, black ticks are invisible on the black canvas.
  if (this->show_face_) {
    lv_draw_rect_dsc_t face;
    lv_draw_rect_dsc_init(&face);
    face.radius = LV_RADIUS_CIRCLE;
    face.bg_color = to_lv(this->face_fill_color_());
    face.bg_opa = LV_OPA_COVER;
    face.border_color = to_lv(this->face_border_color_());
    face.border_width = std::max(1, R / 40);
    face.border_opa = LV_OPA_COVER;
    lv_area_t area = {cx - R, cy - R, cx + R, cy + R};
    lv_draw_rect(&layer, &face, &area);
  }
  if (this->analog_ticks_) {
    for (int i = 0; i < 60; i++) {
      bool hour = (i % 5 == 0);
      float a = i * 6.0f * PI_F / 180.0f;
      float inner = hour ? 0.74f : 0.89f;
      ln.width = hour ? std::max(2, R / 18) : std::max(1, R / 60);
      ln.p1.x = (lv_value_precise_t) (cx + sinf(a) * R * inner);
      ln.p1.y = (lv_value_precise_t) (cy - cosf(a) * R * inner);
      ln.p2.x = (lv_value_precise_t) (cx + sinf(a) * R * 0.96f);
      ln.p2.y = (lv_value_precise_t) (cy - cosf(a) * R * 0.96f);
      lv_draw_line(&layer, &ln);
    }
  }
  int hh, mm, ss;
  if (this->now_hms_(hh, mm, ss)) {
    lv_draw_line_dsc_t hand;
    lv_draw_line_dsc_init(&hand);
    hand.color = to_lv(this->pointer_color_());
    hand.round_start = hand.round_end = true;
    float cs = ss + this->sub_second_(ss);
    float cm = mm + cs / 60.0f;
    float ch = (hh % 12) + cm / 60.0f;
    hand.width = std::max(this->hand_width_ + 2, R / 12);
    this->canvas_hand_(&layer, &hand, cx, cy, (int) (R * 0.55f), ch * 30.0f);
    hand.width = std::max(this->hand_width_ + 1, R / 16);
    this->canvas_hand_(&layer, &hand, cx, cy, (int) (R * 0.82f), cm * 6.0f);
    if (this->analog_seconds_) {
      float sec_a = cs * 6.0f;
      lv_color_t sc = to_lv(this->second_color_());
      hand.color = sc;
      hand.width = std::max(1, R / 50);
      this->canvas_hand_(&layer, &hand, cx, cy, (int) (R * 0.90f), sec_a);
      lv_draw_rect_dsc_t dot;
      lv_draw_rect_dsc_init(&dot);
      dot.radius = LV_RADIUS_CIRCLE;
      dot.bg_color = sc;
      dot.bg_opa = LV_OPA_COVER;
      int pr = std::max(2, R / 11);
      float ar = sec_a * PI_F / 180.0f;
      int ppx = cx + (int) lroundf(sinf(ar) * R * 0.62f);
      int ppy = cy - (int) lroundf(cosf(ar) * R * 0.62f);
      lv_area_t da = {ppx - pr, ppy - pr, ppx + pr, ppy + pr};
      lv_draw_rect(&layer, &dot, &da);
    }
    lv_draw_rect_dsc_t hubd;
    lv_draw_rect_dsc_init(&hubd);
    hubd.radius = LV_RADIUS_CIRCLE;
    hubd.bg_color = to_lv(this->pointer_color_());
    hubd.bg_opa = LV_OPA_COVER;
    int hr = std::max(2, R / 14);
    lv_area_t ha = {cx - hr, cy - hr, cx + hr, cy + hr};
    lv_draw_rect(&layer, &hubd, &ha);
  }
  lv_canvas_finish_layer(canvas, &layer);
}

static void capsule_canvas_(lv_layer_t *layer, lv_draw_rect_dsc_t *rd, int x, int y, int w, int h,
                            lv_color_t col) {
  rd->bg_color = col;
  rd->radius = std::min(w, h) / 2;  // rounded ends
  lv_area_t a = {x, y, x + w - 1, y + h - 1};
  lv_draw_rect(layer, rd, &a);
}

void DisplayClock::canvas_digital_(lv_obj_t *canvas, int w, int h) {
  auto to_lv = [](Color c) { return lv_color_make(c.r, c.g, c.b); };
  lv_canvas_fill_bg(canvas, to_lv(this->background_), LV_OPA_COVER);
  DigitalCell cells[8];
  int n = this->digital_cells_(0, 0, w, h, cells);
  lv_color_t fg = to_lv(this->pointer_color_());
  lv_color_t off = to_lv(this->digital_off_color_());

  lv_layer_t layer;
  lv_canvas_init_layer(canvas, &layer);
  lv_draw_rect_dsc_t rd;
  lv_draw_rect_dsc_init(&rd);
  rd.bg_opa = LV_OPA_COVER;
  lv_draw_rect_dsc_t dot;
  lv_draw_rect_dsc_init(&dot);
  dot.radius = LV_RADIUS_CIRCLE;
  dot.bg_opa = LV_OPA_COVER;

  for (int i = 0; i < n; i++) {
    DigitalCell &c = cells[i];
    if (c.val == 10 || c.val == 11) {  // colon on / ghost
      dot.bg_color = (c.val == 10) ? fg : off;
      int r = std::max(1, c.w / 3), mx = c.x + c.w / 2, y1 = c.y + c.h / 3, y2 = c.y + 2 * c.h / 3;
      lv_area_t a1 = {mx - r, y1 - r, mx + r, y1 + r};
      lv_area_t a2 = {mx - r, y2 - r, mx + r, y2 + r};
      lv_draw_rect(&layer, &dot, &a1);
      lv_draw_rect(&layer, &dot, &a2);
    } else if (c.val >= 0 || c.val == -2) {
      int rects[7][4];
      bool act[7];
      this->seg_rects_(c.val, c.x, c.y, c.w, c.h, rects, act);
      for (int sg = 0; sg < 7; sg++)
        capsule_canvas_(&layer, &rd, rects[sg][0], rects[sg][1], rects[sg][2], rects[sg][3],
                        act[sg] ? fg : off);
    }
  }
  lv_canvas_finish_layer(canvas, &layer);
}

#endif  // USE_LVGL

}  // namespace display_clock
}  // namespace esphome
