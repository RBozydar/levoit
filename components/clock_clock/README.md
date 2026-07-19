# ClockClock 24 — ESPHome display component

A digital clock rendered out of **24 tiny analogue clocks** (4 digits × 2 columns
× 3 rows). Each little clock has two equal-length hands; by pointing the hands
the right way the clocks form the strokes of the digits. When the time changes,
every hand **sweeps clockwise** to its new position with an ease-in-out
animation — the same effect as the well-known
[ClockClock 24](https://clockclock.com/) art piece and Manuel Wieser's
[CodePen](https://codepen.io/Lorti/pen/XpQewQ).

This is a **drawable helper**, not a display driver. You bring your own
`display:` and call `id(cc).draw(it);` from its lambda — the same pattern as the
built-in [`graph`](https://esphome.io/components/graph/) component. That means it
works on any ESPHome-supported display (OLED, e-paper, TFT, LED matrix …).

## Usage

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/tuct/esphome-projects
      ref: main
    components: [clock_clock]

time:
  - platform: sntp
    id: sntp_time
    timezone: Europe/Berlin

clock_clock:
  id: cc
  time_id: sntp_time

display:
  - platform: ssd1306_i2c
    model: "SSD1306 128x64"
    update_interval: 33ms      # ~30 fps for smooth movement
    lambda: 'id(cc).draw(it);'
```

See [`example.yaml`](./example.yaml) for a complete, flashable config.

### Drawing

Call one of these from inside a `display:` lambda:

| Call | Effect |
| --- | --- |
| `id(cc).draw(it);` | Fill the whole display, grid centred. |
| `id(cc).draw(it, x, y, w, h);` | Fit the grid into a rectangle (auto-scaled). |
| `id(cc).draw(it, x, y, w, h, color);` | …with an explicit colour. |

> Put the `draw()` call in the display lambda and set a fast `update_interval`
> (25–40 ms). The animation is advanced in the component's own `loop()`, but the
> screen only shows it as often as the display refreshes.

## Configuration

| Option | Default | Description |
| --- | --- | --- |
| `time_id` | *(required)* | A `time:` component (sntp, ds1307, homeassistant …). |
| `x` / `y` | `0` | Top-left corner of the grid for the no-arg `draw(it)`. |
| `width` / `height` | `0` | Draw area; `0` fills to the display edge from `x`/`y`. |
| `transition_length` | `2s` | How long the hands take to sweep to a new time. |
| `movement` | `opposite` | How hands travel to their target — see below. |
| `mode` | `time` | Initial mode: `time`, `rotate_left`, or `flying_birds`. |
| `mode_speed` | `1.0` | Speed multiplier for the `rotate_left` / `flying_birds` animations. |
| `hand_width` | `1` | Hand thickness in px. `1` suits ~128×64; use `2`–`4` on larger panels. |
| `spacing` | `0.6` | Gap between the HH and MM pairs, in clock-widths. |
| `twenty_four_hour` | `true` | `false` shows 1–12 (leading hour digit blank for 1–9). |
| `show_face` | `false` | Draw a faint circle around each clock. |
| `foreground` | white | Default colour for hands & rims (id of a `color:`). |
| `background` | black | Colour filled *around* the clocks. |
| `clock_pointer` | = `foreground` | Hand colour. |
| `clock_border` | = `foreground` | Clock rim colour (needs `show_face`). |
| `clock_bg` | = `background` | Clock face fill (needs `show_face`). |

### Colours (mono / grayscale / RGB)

All colour options take the **id of a `color:` component** and are optional. On a
mono OLED you can ignore them — the defaults are white-on-black and colours just
collapse to on/off. On grayscale or RGB panels you can style each role:

```yaml
color:
  - { id: c_amber,  red: 100%, green: 65%, blue: 0% }
  - { id: c_navy,   red: 4%,   green: 6%,  blue: 16% }
  - { id: c_rim,    red: 20%,  green: 22%, blue: 30% }

clock_clock:
  id: cc
  time_id: sntp_time
  show_face: true
  background: c_navy       # around the clocks
  clock_bg: c_navy         # each clock face
  clock_border: c_rim      # each clock rim
  clock_pointer: c_amber   # the hands
```

Unset roles fall back sensibly: `clock_pointer`/`clock_border` → `foreground`,
`clock_bg` → `background`. `background` fills the drawn rectangle every frame, so
the component owns its area (don't overlay it on other content unless that region
is meant to be cleared).

### Movement — how the hands travel

The two hands of each clock are the same length, so *how* they rotate to a new
digit is the whole personality of the piece:

| `movement` | Feel |
| --- | --- |
| `opposite` *(default)* | The two hands **counter-rotate** (one clockwise, one anti-clockwise) — the signature ClockClock "scissor" sweep. |
| `clockwise` | Both hands take the shortest clockwise path. |
| `counter` | Both hands take the shortest anti-clockwise path. |
| `long` | Both hands always take the long way round (> 180°) — very dramatic. |

## Modes & animations

Besides showing the time, the display has two idle animations:

- **`rotate_left`** — a synchronised spinner (a full line rotating anti-clockwise
  on every clock). Great as a "connecting / loading" screen.
- **`flying_birds`** — every clock becomes a flapping seagull silhouette; a
  per-clock phase offset makes it look like a drifting flock.

Switch modes from YAML **actions** (there is no network dependency baked into the
component — you decide when each phase runs):

| Action | Effect |
| --- | --- |
| `clock_clock.show_time: cc` | Sweep from wherever the hands are into the current time. |
| `clock_clock.rotate_left: cc` | Start the spinner. |
| `clock_clock.flying_birds: cc` | Start the flock. |

### Example: boot phases (spin → birds → clock)

```yaml
clock_clock:
  id: cc
  time_id: sntp_time
  mode: rotate_left          # start spinning at boot

interval:
  - interval: 1s
    then:
      - if:
          condition:
            lambda: 'return id(sntp_time).now().is_valid();'
          then:
            - clock_clock.show_time: cc         # time is set
          else:
            - if:
                condition:
                  wifi.connected:
                then:
                  - clock_clock.flying_birds: cc  # online, waiting for NTP
                else:
                  - clock_clock.rotate_left: cc   # still connecting
```

## Resolution — how small can it go?

The grid is **8 clocks wide** (plus the HH/MM gap) and **3 tall**, and it
auto-scales to whatever area you draw into. A hand needs roughly a 14–16 px
clock to stay readable, so:

| Display | Result |
| --- | --- |
| **128 × 64** (SSD1306) | ✅ Practical minimum — all digits legible (clock ≈ 15 px). |
| **128 × 32** | ⚠️ Too short — 3 rows only get ~10 px each; hands blur together. |
| **256 × 128 and up** | 👍 Recommended — crisp; bump `hand_width` to 3–4. |

`dump_config` prints the recommended minimum for your exact `spacing`
(`min_width ≈ ceil(16 × (8 + spacing))`, `min_height = 48`). For a 128-wide
panel keep `spacing` small (≤ 0.6) so the clocks don't get squeezed.

## How the font works

Each clock has only **two hands**, so a true seven-segment font is impossible
(a "3" or "8" would need a three-way junction in a single clock). Instead the
digits use a hand-tuned table where three-way junctions are split across
neighbouring clocks. The one unavoidable compromise is **8**: it keeps a solid
box with the middle bar hung off the right vertical (a small notch on the left)
— still clearly an 8. The full angle table lives in
[`clock_clock.cpp`](./clock_clock.cpp) (`FONT[]`); every glyph was verified by
rendering it before shipping.

## Credits

- Concept: **ClockClock 24** by *Humans since 1982*.
- JS reference implementation: **Manuel Wieser** (“Lorti”).
