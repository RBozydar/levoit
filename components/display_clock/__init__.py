import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import font, time
from esphome.const import (
    CONF_FONT,
    CONF_HEIGHT,
    CONF_ID,
    CONF_MODE,
    CONF_TIME_ID,
    CONF_WIDTH,
    CONF_X,
    CONF_Y,
)

CODEOWNERS = ["@tuct"]
DEPENDENCIES = ["time", "display"]

display_clock_ns = cg.esphome_ns.namespace("display_clock")
DisplayClock = display_clock_ns.class_("DisplayClock", cg.Component)
SetModeAction = display_clock_ns.class_("SetModeAction", automation.Action)
ColorStruct = cg.esphome_ns.struct("Color")

ClockStyle = display_clock_ns.enum("ClockStyle")
ClockMode = display_clock_ns.enum("ClockMode")
MODES = {
    "time": ClockMode.CC_MODE_TIME,
    "rotate_left": ClockMode.CC_MODE_ROTATE_LEFT,
    "flying_birds": ClockMode.CC_MODE_FLYING_BIRDS,
}
MovementMode = display_clock_ns.enum("MovementMode")
MOVEMENTS = {
    "opposite": MovementMode.CC_MOVE_OPPOSITE,
    "clockwise": MovementMode.CC_MOVE_CLOCKWISE,
    "counter": MovementMode.CC_MOVE_COUNTER,
    "long": MovementMode.CC_MOVE_LONG,
}

# style sub-blocks
CONF_CLOCKCLOCK24 = "clockclock24"
CONF_ANALOG = "analog"
CONF_DIGITAL = "digital"
STYLES = {
    CONF_CLOCKCLOCK24: ClockStyle.STYLE_CLOCKCLOCK24,
    CONF_ANALOG: ClockStyle.STYLE_ANALOG,
    CONF_DIGITAL: ClockStyle.STYLE_DIGITAL,
}

# shared
CONF_TWENTY_FOUR_HOUR = "twenty_four_hour"
CONF_HAND_WIDTH = "hand_width"
CONF_ANTI_ALIAS = "anti_alias"
CONF_SHOW_FACE = "show_face"
CONF_FOREGROUND = "foreground"  # the "ink": hands / markers / digits
CONF_BACKGROUND = "background"  # behind everything
# face colours (styles that draw a dial / clock faces)
CONF_FACE_COLOR = "face_color"  # dial / clock-face fill
CONF_BORDER_COLOR = "border_color"  # dial rim + tick marks
# clockclock24
CONF_TRANSITION_LENGTH = "transition_length"
CONF_MOVEMENT = "movement"
CONF_MODE_SPEED = "mode_speed"
CONF_SPACING = "spacing"
# analog / digital
CONF_SECONDS = "seconds"
CONF_TICKS = "ticks"
CONF_NUMERALS = "numerals"
CONF_SECOND_COLOR = "second_color"
CONF_BLINK = "blink"
CONF_BLANK_LEADING_ZERO = "blank_leading_zero"
CONF_OFF_COLOR = "off_color"

# options shared by the styles that draw a face (analog + clockclock24)
FACE_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_SHOW_FACE, default=False): cv.boolean,
        cv.Optional(CONF_FACE_COLOR): cv.use_id(ColorStruct),
        cv.Optional(CONF_BORDER_COLOR): cv.use_id(ColorStruct),
    }
)

CLOCKCLOCK24_SCHEMA = cv.Schema(
    {
        cv.Optional(
            CONF_TRANSITION_LENGTH, default="2s"
        ): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_MOVEMENT, default="opposite"): cv.enum(MOVEMENTS, lower=True),
        cv.Optional(CONF_MODE, default="time"): cv.enum(MODES, lower=True),
        cv.Optional(CONF_MODE_SPEED, default=1.0): cv.positive_float,
        cv.Optional(CONF_SPACING, default=0.6): cv.float_range(min=0.0, max=4.0),
    }
).extend(FACE_SCHEMA)

ANALOG_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_SECONDS, default=False): cv.boolean,
        cv.Optional(CONF_TICKS, default=True): cv.boolean,
        cv.Optional(CONF_NUMERALS, default=False): cv.boolean,
        cv.Optional(CONF_FONT): cv.use_id(font.Font),  # only for numerals
        cv.Optional(CONF_SECOND_COLOR): cv.use_id(ColorStruct),
    }
).extend(FACE_SCHEMA)

DIGITAL_SCHEMA = cv.Schema(
    {
        # Self-contained 7-segment renderer (works on the display and the LVGL
        # canvas).
        cv.Optional(CONF_SECONDS, default=False): cv.boolean,
        cv.Optional(CONF_BLINK, default=False): cv.boolean,
        cv.Optional(CONF_BLANK_LEADING_ZERO, default=False): cv.boolean,
        # colour of unlit segments (the "ghost 8"); default is a dark grey
        cv.Optional(CONF_OFF_COLOR): cv.use_id(ColorStruct),
    }
)


def _exactly_one_style(config):
    present = [k for k in STYLES if k in config]
    if len(present) > 1:
        raise cv.Invalid(f"Only one clock style may be set, got: {', '.join(present)}")
    if not present:
        config[CONF_CLOCKCLOCK24] = CLOCKCLOCK24_SCHEMA({})
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(DisplayClock),
            cv.Required(CONF_TIME_ID): cv.use_id(time.RealTimeClock),
            cv.Optional(CONF_X, default=0): cv.int_,
            cv.Optional(CONF_Y, default=0): cv.int_,
            cv.Optional(CONF_WIDTH, default=0): cv.int_range(min=0),
            cv.Optional(CONF_HEIGHT, default=0): cv.int_range(min=0),
            cv.Optional(CONF_TWENTY_FOUR_HOUR, default=True): cv.boolean,
            cv.Optional(CONF_HAND_WIDTH, default=1): cv.int_range(min=1, max=12),
            cv.Optional(CONF_ANTI_ALIAS, default=False): cv.boolean,
            cv.Optional(CONF_FOREGROUND): cv.use_id(ColorStruct),
            cv.Optional(CONF_BACKGROUND): cv.use_id(ColorStruct),
            cv.Optional(CONF_CLOCKCLOCK24): CLOCKCLOCK24_SCHEMA,
            cv.Optional(CONF_ANALOG): ANALOG_SCHEMA,
            cv.Optional(CONF_DIGITAL): DIGITAL_SCHEMA,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    _exactly_one_style,
)


async def _set_color(var, config, key, setter):
    if key in config:
        cg.add(setter(await cg.get_variable(config[key])))


async def _apply_face(var, c):
    cg.add(var.set_show_face(c[CONF_SHOW_FACE]))
    await _set_color(var, c, CONF_FACE_COLOR, var.set_face_fill_color)
    await _set_color(var, c, CONF_BORDER_COLOR, var.set_face_border_color)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_time(await cg.get_variable(config[CONF_TIME_ID])))
    cg.add(var.set_position(config[CONF_X], config[CONF_Y]))
    cg.add(var.set_size(config[CONF_WIDTH], config[CONF_HEIGHT]))
    cg.add(var.set_twenty_four_hour(config[CONF_TWENTY_FOUR_HOUR]))
    cg.add(var.set_hand_width(config[CONF_HAND_WIDTH]))
    cg.add(var.set_anti_alias(config[CONF_ANTI_ALIAS]))
    await _set_color(var, config, CONF_FOREGROUND, var.set_foreground)
    await _set_color(var, config, CONF_BACKGROUND, var.set_background)

    if CONF_CLOCKCLOCK24 in config:
        c = config[CONF_CLOCKCLOCK24]
        cg.add(var.set_style(STYLES[CONF_CLOCKCLOCK24]))
        cg.add(var.set_transition_length(c[CONF_TRANSITION_LENGTH].total_milliseconds))
        cg.add(var.set_movement(c[CONF_MOVEMENT]))
        cg.add(var.set_mode_speed(c[CONF_MODE_SPEED]))
        cg.add(var.set_spacing(c[CONF_SPACING]))
        cg.add(var.set_mode(c[CONF_MODE]))
        await _apply_face(var, c)
    elif CONF_ANALOG in config:
        c = config[CONF_ANALOG]
        cg.add(var.set_style(STYLES[CONF_ANALOG]))
        cg.add(var.set_analog_seconds(c[CONF_SECONDS]))
        cg.add(var.set_analog_ticks(c[CONF_TICKS]))
        cg.add(var.set_analog_numerals(c[CONF_NUMERALS]))
        if CONF_FONT in c:
            cg.add(var.set_analog_font(await cg.get_variable(c[CONF_FONT])))
        await _set_color(var, c, CONF_SECOND_COLOR, var.set_second_color)
        await _apply_face(var, c)
    elif CONF_DIGITAL in config:
        c = config[CONF_DIGITAL]
        cg.add(var.set_style(STYLES[CONF_DIGITAL]))
        cg.add(var.set_digital_seconds(c[CONF_SECONDS]))
        cg.add(var.set_digital_blink(c[CONF_BLINK]))
        cg.add(var.set_digital_blank_leading(c[CONF_BLANK_LEADING_ZERO]))
        await _set_color(var, c, CONF_OFF_COLOR, var.set_digital_off_color)


# --- Actions (clockclock24 idle modes) --------------------------------------
_ACTION_SCHEMA = automation.maybe_simple_id({cv.GenerateID(): cv.use_id(DisplayClock)})


def _register_mode_action(action, mode):
    @automation.register_action(action, SetModeAction, _ACTION_SCHEMA, synchronous=True)
    async def _to_code(config, action_id, template_arg, args):
        parent = await cg.get_variable(config[CONF_ID])
        return cg.new_Pvariable(action_id, template_arg, parent, mode)

    return _to_code


_show_time = _register_mode_action("display_clock.show_time", MODES["time"])
_rotate_left = _register_mode_action("display_clock.rotate_left", MODES["rotate_left"])
_flying_birds = _register_mode_action("display_clock.flying_birds", MODES["flying_birds"])
