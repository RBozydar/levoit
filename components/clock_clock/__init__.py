import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import time
from esphome.const import (
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

clock_clock_ns = cg.esphome_ns.namespace("clock_clock")
ClockClock = clock_clock_ns.class_("ClockClock", cg.Component)
SetModeAction = clock_clock_ns.class_("SetModeAction", automation.Action)

# esphome::Color - matches ids declared by the `color:` component.
ColorStruct = cg.esphome_ns.struct("Color")

ClockClockMode = clock_clock_ns.enum("ClockClockMode")
MODES = {
    "time": ClockClockMode.CC_MODE_TIME,
    "rotate_left": ClockClockMode.CC_MODE_ROTATE_LEFT,
    "flying_birds": ClockClockMode.CC_MODE_FLYING_BIRDS,
}

MovementMode = clock_clock_ns.enum("MovementMode")
MOVEMENTS = {
    "opposite": MovementMode.CC_MOVE_OPPOSITE,
    "clockwise": MovementMode.CC_MOVE_CLOCKWISE,
    "counter": MovementMode.CC_MOVE_COUNTER,
    "long": MovementMode.CC_MOVE_LONG,
}

CONF_TRANSITION_LENGTH = "transition_length"
CONF_SPACING = "spacing"
CONF_HAND_WIDTH = "hand_width"
CONF_SHOW_FACE = "show_face"
CONF_TWENTY_FOUR_HOUR = "twenty_four_hour"
CONF_MOVEMENT = "movement"
CONF_MODE_SPEED = "mode_speed"
CONF_FOREGROUND = "foreground"
CONF_BACKGROUND = "background"
CONF_CLOCK_BG = "clock_bg"
CONF_CLOCK_BORDER = "clock_border"
CONF_CLOCK_POINTER = "clock_pointer"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(ClockClock),
        cv.Required(CONF_TIME_ID): cv.use_id(time.RealTimeClock),
        cv.Optional(
            CONF_TRANSITION_LENGTH, default="2s"
        ): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_MOVEMENT, default="opposite"): cv.enum(MOVEMENTS, lower=True),
        cv.Optional(CONF_MODE, default="time"): cv.enum(MODES, lower=True),
        cv.Optional(CONF_MODE_SPEED, default=1.0): cv.positive_float,
        cv.Optional(CONF_X, default=0): cv.int_,
        cv.Optional(CONF_Y, default=0): cv.int_,
        # width/height of 0 (default) fills to the display edge from x/y.
        cv.Optional(CONF_WIDTH, default=0): cv.int_range(min=0),
        cv.Optional(CONF_HEIGHT, default=0): cv.int_range(min=0),
        cv.Optional(CONF_SPACING, default=0.6): cv.float_range(min=0.0, max=4.0),
        cv.Optional(CONF_HAND_WIDTH, default=1): cv.int_range(min=1, max=10),
        cv.Optional(CONF_SHOW_FACE, default=False): cv.boolean,
        cv.Optional(CONF_TWENTY_FOUR_HOUR, default=True): cv.boolean,
        # Colours: reference ids from the `color:` component. Optional; sensible
        # mono defaults apply when omitted (white on black).
        cv.Optional(CONF_FOREGROUND): cv.use_id(ColorStruct),
        cv.Optional(CONF_BACKGROUND): cv.use_id(ColorStruct),
        cv.Optional(CONF_CLOCK_BG): cv.use_id(ColorStruct),
        cv.Optional(CONF_CLOCK_BORDER): cv.use_id(ColorStruct),
        cv.Optional(CONF_CLOCK_POINTER): cv.use_id(ColorStruct),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    time_var = await cg.get_variable(config[CONF_TIME_ID])
    cg.add(var.set_time(time_var))
    cg.add(
        var.set_transition_length(config[CONF_TRANSITION_LENGTH].total_milliseconds)
    )
    cg.add(var.set_movement(config[CONF_MOVEMENT]))
    cg.add(var.set_mode_speed(config[CONF_MODE_SPEED]))
    cg.add(var.set_position(config[CONF_X], config[CONF_Y]))
    cg.add(var.set_size(config[CONF_WIDTH], config[CONF_HEIGHT]))
    cg.add(var.set_spacing(config[CONF_SPACING]))
    cg.add(var.set_hand_width(config[CONF_HAND_WIDTH]))
    cg.add(var.set_show_face(config[CONF_SHOW_FACE]))
    cg.add(var.set_twenty_four_hour(config[CONF_TWENTY_FOUR_HOUR]))
    cg.add(var.set_mode(config[CONF_MODE]))

    for key, setter in (
        (CONF_FOREGROUND, var.set_foreground),
        (CONF_BACKGROUND, var.set_background),
        (CONF_CLOCK_BG, var.set_clock_bg_color),
        (CONF_CLOCK_BORDER, var.set_border_color),
        (CONF_CLOCK_POINTER, var.set_pointer_color),
    ):
        if key in config:
            col = await cg.get_variable(config[key])
            cg.add(setter(col))


# --- Actions: switch mode from YAML automations -----------------------------
_ACTION_SCHEMA = automation.maybe_simple_id(
    {cv.GenerateID(): cv.use_id(ClockClock)}
)


def _register_mode_action(action, mode):
    @automation.register_action(
        action, SetModeAction, _ACTION_SCHEMA, synchronous=True
    )
    async def _to_code(config, action_id, template_arg, args):
        parent = await cg.get_variable(config[CONF_ID])
        return cg.new_Pvariable(action_id, template_arg, parent, mode)

    return _to_code


_show_time = _register_mode_action("clock_clock.show_time", MODES["time"])
_rotate_left = _register_mode_action("clock_clock.rotate_left", MODES["rotate_left"])
_flying_birds = _register_mode_action(
    "clock_clock.flying_birds", MODES["flying_birds"]
)
