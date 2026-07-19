# display_clock — a clock for any ESPHome display

Draws a clock onto any ESPHome-supported display, in one of three **styles**:

| `style` | What it looks like |
| --- | --- |
| **clockclock24** *(default)* | A digital clock built from **24 tiny analogue clocks** ([ClockClock 24](https://clockclock.com/)); hands sweep to form the digits, with `rotate_left` / `flying_birds` idle animations. |
| **analog** | A single **Swiss railway (SBB)** clock — bold bar markers, baton hands, red "lollipop" second hand; all hands sweep continuously. |
| **digital** | `HH:MM(:SS)` as a rounded **7-segment** display with a "ghost 8", optional blinking colon. |

It's a *drawable helper*, not a display driver: call `id(dc).draw(it);` from a
display lambda, like the `graph` component. It also draws into an **LVGL 9
canvas** (`draw_to_canvas`).

## Usage

```yaml
external_components:
  - source: { type: git, url: https://github.com/tuct/esphome-projects, ref: main }
    components: [display_clock]

time:
  - platform: sntp
    id: sntp_time
    timezone: "CET-1CEST,M3.5.0,M10.5.0/3"

display_clock:
  id: dc
  time_id: sntp_time
  analog:
    seconds: true

display:
  - platform: ssd1306_i2c
    model: "SH1106 128x64"
    update_interval: 33ms
    lambda: "id(dc).draw(it);"
```

The **style is chosen by which sub-block is present** (`clockclock24:` /
`analog:` / `digital:`) — exactly one. Omit them all to get `clockclock24`.
See [`example.local.yaml`](./example.local.yaml).

### Quick start: include a ready-made style

Instead of writing the block yourself, `!include` one of the per-style files —
each bundles the full `display_clock:` block plus its colours:

```yaml
packages:
  clock: !include display_clock/display_clock_analog.yaml   # or _digital / _clockclock24
```

You still provide a `time:` (default id `sntp_time`), the display, and the draw
call. Override the ids from your own config if needed:

```yaml
substitutions:
  clock_id: dc
  clock_time_id: my_time
```

Files: [`_clockclock24`](./display_clock_clockclock24.yaml),
[`_analog`](./display_clock_analog.yaml),
[`_digital`](./display_clock_digital.yaml).

## Shared options

| Option | Default | Description |
| --- | --- | --- |
| `time_id` | *(required)* | A `time:` component. |
| `x` / `y` | `0` | Top-left of the clock for the no-arg `draw(it)`. |
| `width` / `height` | `0` | Draw area; `0` = fill to the display edge. |
| `twenty_four_hour` | `true` | `false` = 1–12. |
| `hand_width` | `1` | Base hand thickness (px). |
| `anti_alias` | `false` | Smooth diagonal hands (Xiaolin Wu). Only visible on colour/grayscale panels. |
| `foreground` | white | The "ink": hands / markers / digits. |
| `background` | black | Behind everything. |

`foreground` and `background` are the only **shared** colours (they mean the
same thing in every style). Face-specific colours live inside the style block
that uses them — see below. All colours take the **id of a `color:` component**
and are optional; omit for white-on-black, which also collapses to on/off on
mono panels.

## Style: `clockclock24`

| Option | Default | Description |
| --- | --- | --- |
| `movement` | `opposite` | `opposite` (counter-rotating hands), `clockwise`, `counter`, `long`. |
| `transition_length` | `2s` | Sweep duration on a time change. |
| `mode` | `time` | Initial mode: `time`, `rotate_left`, `flying_birds`. |
| `mode_speed` | `1.0` | Idle-animation speed multiplier. |
| `spacing` | `0.6` | Gap between HH and MM, in clock-widths. |
| `show_face` | `false` | Circle around each mini-clock. |
| `face_color` / `border_color` | bg / fg | Little-clock fill & rim (with `show_face`). |

**Idle-animation actions** (drive from automations): `display_clock.show_time`,
`display_clock.rotate_left`, `display_clock.flying_birds` — e.g. spin while
Wi-Fi connects, birds while waiting for NTP, then the time. See the `interval`
in the example.

## Style: `analog`

Swiss railway look, with **continuous sweep** — all three hands glide (no
ticking or stop-to-go pause). Hour/minute hands always; the rest optional:

| Option | Default | Description |
| --- | --- | --- |
| `show_face` | `false` | Draw the dial circle. |
| `seconds` | `false` | Red lollipop second hand. |
| `ticks` | `true` | 60 minute marks with bold hour bars. |
| `numerals` | `false` | Draw 1–12 (needs `font:`). |
| `font` | — | Font id for the numerals. |
| `face_color` / `border_color` | bg / fg | Dial fill & the rim + tick marks. |
| `second_color` | red | The lollipop second hand. |

## Style: `digital`

Self-contained **7-segment** display with rounded-end (capsule) segments — no
font needed, works on the **display** and the **LVGL canvas**.

| Option | Default | Description |
| --- | --- | --- |
| `seconds` | `false` | `HH:MM:SS` instead of `HH:MM`. |
| `blink` | `false` | Colon blinks (to the `off_color` "ghost"). |
| `blank_leading_zero` | `false` | Hide the leading hour zero. |
| `off_color` | dark grey | Colour of *unlit* segments — the classic "ghost 8". Set it to `background` to hide them. |

Colours: uses the shared `foreground` (lit segments) and `background`.

## Resolution

Auto-scales to the draw area; the first draw logs the actual size vs the
recommended minimum, per style:

| Style | Practical minimum |
| --- | --- |
| clockclock24 | ~128×48 (128×64 OLED is the sweet spot) |
| analog | ~32×32 (bigger = more detail) |
| digital | font-dependent |

## LVGL

`id(dc).draw_to_canvas(id(my_canvas));` renders into an LVGL 9 `canvas` widget
(clockclock24 and analog styles). See [`example.lvgl.yaml`](./example.lvgl.yaml).
For a digital clock under LVGL, use a native `label` instead.

## Credits

ClockClock 24 by *Humans since 1982*; JS reference by *Manuel Wieser*. Analog
face after the *SBB* Swiss railway clock (Hans Hilfiker, 1944).
