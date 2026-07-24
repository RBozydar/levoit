# display_clock — a clock for ESPHome's LVGL

A native [LVGL 9](https://esphome.io/components/lvgl/#widgets) widget: add it
under `lvgl: widgets:` just like `canvas` or `line`. It owns its own canvas
and redraws itself — no `interval:` + lambda glue needed. Pick a **style**:

| `style` | What it looks like |
| --- | --- |
| **clockclock24** *(default)* | A digital clock built from **24 tiny analogue clocks** ([ClockClock 24](https://clockclock.com/)); hands sweep to form the digits, with `rotate_left` / `flying_birds` idle animations. |
| **analog** | A classic analogue clock face — independently configurable ticks and per-hand style/colour (see below). |
| **digital** | `HH:MM(:SS)` as a rounded **7-segment** display with a "ghost 8", optional blinking colon. |
| **flipclock** | `HH:MM(:SS)` as **split-flap cards** with real font-rendered digits and an animated flip on every change ([flipclock.js](https://flipclockjs.com/) look). |

## Usage

```yaml
external_components:
  - source: { type: git, url: https://github.com/tuct/esphome-projects, ref: main }
    components: [display_clock]

time:
  - platform: sntp
    id: sntp_time
    timezone: "CET-1CEST,M3.5.0,M10.5.0/3"

# Marker only - enables the widget below, takes no options itself. Required
# because ESPHome only loads a component's code when its domain appears as a
# top-level YAML key.
display_clock:

lvgl:
  displays: [my_display]
  widgets:
    - display_clock:
        id: dc
        time_id: sntp_time
        width: 150
        height: 128
        style: analog
        show_seconds: true
```

See [`example.lvgl.yaml`](./example.lvgl.yaml) for a full working config
(display + LVGL setup + boot-phase idle animations). There's one per style -
[`example.lvgl.yaml`](./example.lvgl.yaml) (clockclock24),
[`example_analog.yaml`](./example_analog.yaml),
[`example_digital.yaml`](./example_digital.yaml),
[`example_flipclock.yaml`](./example_flipclock.yaml) - plus
[`example_demo.yaml`](./example_demo.yaml) for the clockclock24 test mode
below. They share their hardware/network setup via
[`common_base.yaml`](./common_base.yaml) and their LVGL/colour setup via
[`common_lvgl.yaml`](./common_lvgl.yaml), pulled in with `packages:` +
`!include` - each example itself is just its `esphome: name:` plus the
`lvgl: widgets: - display_clock: ...` block for that style.

Position and size (`x`, `y`, `width`, `height`, `align`, ...) are ordinary
LVGL widget properties — set them like on any other widget. `width`/`height`
are required here (used to size the canvas buffer).

The **style is chosen by the `style:` key** — `clockclock24` / `analog` /
`digital` / `flipclock`. If omitted, it's inferred from whichever one style
sub-block (`clockclock24:` / `analog:` / `digital:` / `flipclock:`) you
provide, defaulting to `clockclock24` if none are given.

## Shared options

| Option | Default | Description |
| --- | --- | --- |
| `time_id` | *(required)* | A `time:` component. |
| `width` / `height` | *(required)* | Canvas size in px. |
| `style` | `clockclock24` | `clockclock24`, `analog`, `digital`, or `flipclock`. |
| `twenty_four_hour` | `true` | `false` = 1–12. |
| `show_seconds` | `false` | Adds `:SS` (`digital`/`flipclock`) or a sweeping second hand (`analog`). **No-op for `clockclock24`** — the physical ClockClock 24 has no seconds display. |
| `show_date` / `show_date_format` | `false` / `short` | Accepted, but **not yet rendered** — reserved for a future date-display feature. |
| `render_interval` | `33ms` | How often the widget redraws itself. |
| `foreground` | white | The "ink": hands / markers / digits. |
| `background` | black | Behind everything. |

Before the time source first syncs, `analog` / `digital` / `flipclock` show a
fake **00:15** with the seconds running off uptime, so the clock looks alive
instead of showing dashes. `clockclock24` instead parks its hands until the
first sync — or runs whatever idle animation you drive it with during the
wifi/NTP boot phase (see its mode actions below).

`foreground` and `background` are the only **shared** colours (they mean the
same thing in every style). Face-specific colours live inside the style
block that uses them — see below. All colours take the **id of a `color:`
component** and are optional; omit for white-on-black.

## `style: clockclock24`

```yaml
clockclock24:
  hand_width: 1             # base hand thickness (px)
  movement: opposite        # opposite | clockwise | counter | long
  transition_length: 2s      # sweep duration on a time change
  mode: time                 # time | rotate_left | flying_birds | demo
  mode_speed: 1.0             # idle-animation speed multiplier (rotate_left/flying_birds only)
  spacing: 0.6                # gap between HH and MM, in clock-widths
  demo_interval: 5s           # `mode: demo` only - see below
  show_face: false            # draw a filled disc behind each mini-clock's hands
  face_color: ...             # little-clock fill (needs show_face)
  border_color: ...           # little-clock rim (needs show_face)
```

Each mini-clock has two hands, and `movement` controls which direction each
takes to reach its new target angle:

- `opposite` *(default)* — the two hands travel in opposite directions from
  each other (one clockwise, one counter-clockwise) — the classic ClockClock
  24 look, hands seeming to open/close like scissors.
- `clockwise` — both hands always travel clockwise.
- `counter` — both hands always travel counter-clockwise.
- `long` — each hand independently takes whichever direction is the *longer*
  way around, for a more dramatic full sweep.

**Idle-animation actions** (drive from automations): `display_clock.show_time`,
`display_clock.rotate_left`, `display_clock.flying_birds` — e.g. spin while
Wi-Fi connects, birds while waiting for NTP, then the time. See
[`example.lvgl.yaml`](./example.lvgl.yaml).

**Testing the digit-flip animation** — clockclock24 only re-animates on an
actual minute change, so watching it live can mean waiting up to 60 real
seconds to see a flip. `mode: demo` (action: `display_clock.demo`) advances a
fake internal minute every `demo_interval` (default `5s`) instead of reading
the real clock, so you can watch the animation repeatedly on demand. See
[`example_demo.yaml`](./example_demo.yaml) for a config that boots straight
into it.

## `style: analog`

Continuous sweep — all hands glide (no ticking or stop-to-go pause). Hands and
ticks are independently configurable, so this one style covers everything
from a bare minimalist face (no ticks, thin second hand) to a fully ticked
watch face:

```yaml
analog:
  show_face: false            # draw the dial circle behind the hands
  minute_ticks:               # 60 small 1-min ticks
    enabled: true
    color: ...                  # defaults to `border_color`
    rounded: true                 # rounded vs flat/square tick ends
    width: m                       # s | m | l
    length: m                      # s | m | l
  hour_ticks:                 # 12 bold 5-min/hour ticks
    enabled: true
    color: ...
    rounded: true
    width: m
    length: m
  face_color: ...             # dial fill (needs show_face)
  border_color: ...           # dial rim + default tick colour (needs show_face)
  hour_hand:
    style: baton               # baton | line | lollipop | sbb
    color: ...                  # defaults to `foreground`
    center_style: circle         # circle | round | none
    extend: 0%                    # extend the hand past the pivot, max 50%
  minute_hand:
    style: baton
    color: ...
    center_style: circle
    extend: 0%
  second_hand:
    style: lollipop             # needs `show_seconds: true` (shared option)
    color: ...                  # defaults to red
    center_style: circle
    extend: 20%                # a short counterweight tail, like a real second hand
```

`minute_ticks`/`hour_ticks` are independent: `enabled` toggles that ring on/off
(if `hour_ticks.enabled: false` but `minute_ticks.enabled: true`, the minute
styling fills in at the hour positions too, so you still get a full ring
instead of 12 gaps); `color` overrides it (defaults to `border_color`);
`rounded` picks flat/square vs rounded tick ends; `width`/`length` pick a
size (`s`/`m`/`l`) — the scale is shared, so `s`/`m`/`l` mean the same
absolute size on either ring. Each ring defaults to its own historical look
(`minute_ticks`: `s`, `hour_ticks`: `l`) unless overridden.

Each hand is fully independent:
- `style` picks the shape — `baton`: a tapered stalk into a thick rounded bar
  (circle -> line -> rounded rectangle, doesn't start flush at the pivot);
  `line`: a thin plain line; `lollipop`: a thin line ending inside a solid
  ball ~65% of the way along it (the classic Mondaine/SBB second-hand look);
  `sbb`: a plain, only slightly tapered rectangle with flat-cut ends — the
  classic Swiss railway hour/minute hand, intended for those two hands.
- `color` overrides the hand's colour (defaults to `foreground`, or red for
  `second_hand`).
- `center_style` picks how *that hand's own* centre marker looks: `circle`
  *(default)* draws a ring in the hand's colour around a black centre; `round`
  draws a plain filled circle in the hand's colour; `none` draws nothing.
  Each hand renders fully (shape, then its own marker) before the next one
  starts — hour, then minute on top of it, then second on top of both, like a
  real watch — so with all three left at `circle` you get a layered
  black/ring/ring hub; set the ones you don't want to `none`.
- `extend` stretches the hand a little past the pivot on the *opposite* side
  (max 50%) — e.g. the second hand's small counterweight tail.

`minute_ticks`/`hour_ticks` toggle the two tick rings separately, each with
its own colour and rounded override.

## `style: digital`

Self-contained **7-segment** display — no font needed.

```yaml
digital:
  segment_style: classic     # classic | rounded
  blink: false               # colon blinks (to the off_color "ghost")
  blank_leading_zero: false  # hide the leading hour zero
  off_color: ...             # colour of *unlit* segments - the classic "ghost 8"
```

`segment_style` picks the shape of each of the 7 bars: `classic` *(default)*
tapers each end to a point, meeting the neighbouring segment's point at the
digit's corners — the traditional LCD/calculator look. `rounded` uses fully
rounded capsule ends instead.

In 12h mode (`twenty_four_hour: false`) an **AM/PM** marker column appears on
the left, like on a real LED clock module: AM on top, PM below, the active
one lit and the other shown in the `off_color` ghost. The letters are drawn
as vector strokes (no font needed) and auto-scale with the widget like the
digits themselves.

## `style: flipclock`

Split-flap ("Solari") cards, one digit per card, with a horizontal seam and
an animated flip on every digit change — the [flipclock.js](https://flipclockjs.com/)
look. Unlike `digital` this renders **real font glyphs**, so a `font:` is
required:

```yaml
flipclock:
  font: montserrat_48        # built-in LVGL font name, or an ESPHome font: id
  card_color: ...            # card fill - defaults to a dark grey
  flip_duration: 450ms       # one digit flip; 0 disables the animation
  blink: false               # divider dots blink off every other second
  blank_leading_zero: false  # hide the leading hour zero (blank card)
  show_dots: true            # false = no divider dots, just a gap between groups
  am_pm_font: montserrat_14  # 12h mode only - small AM/PM marker font
```

- `font` takes either a **built-in LVGL font** (`montserrat_8` …
  `montserrat_48` — the validator enables it in the LVGL build
  automatically) or the id of an ESPHome
  [`font:`](https://esphome.io/components/font/) component, for any size or
  typeface. The glyph size is fixed by the font, so match it to the widget:
  the cards themselves scale to `width`/`height`, the digits don't.
- Digit colour is the shared `foreground`; the gaps between cards and the
  seam line show `background`.
- The flip is the classic two-phase flap: the top half falls over the old
  digit, then lands on the bottom half revealing the new one, easing in like
  a real gravity-driven flap.
- In 12h mode (`twenty_four_hour: false`) a dedicated **AM/PM** card is added
  in front of the hours, like on a real flip clock, with the marker sitting
  centred in the lower half of the card — `am_pm_font` sizes its two-letter
  text (any built-in LVGL font or ESPHome `font:` id; defaults to a small one).

## Resolution

Auto-scales to the draw area; the first render logs the actual size vs the
recommended minimum, per style:

| Style | Practical minimum |
| --- | --- |
| clockclock24 | ~128×48 (128×64 OLED is the sweet spot) |
| analog | ~32×32 (bigger = more detail) |
| digital | ~24×12 (the 7-segment renderer is self-contained - no font, scales freely) |
| flipclock | font-dependent - the cards scale, the glyphs don't; pick a font that fits |

## Credits

ClockClock 24 by *Humans since 1982*;
JS reference by *Manuel Wieser*.
Analog face after the *SBB* Swiss railway clock (Hans Hilfiker, 1944).
