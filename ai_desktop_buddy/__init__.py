import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.const import (
    CONF_DISABLED_BY_DEFAULT,
    CONF_FORCE_UPDATE,
    CONF_ID,
    CONF_NAME,
)
from esphome.components import  esp32_ble, sensor, text_sensor, binary_sensor
from esphome.components.esp32_ble import BTLoggers
from esphome.components.lvgl.helpers import add_lv_use
from esphome.components.lvgl.types import lv_obj_t
from esphome.core import ID

CODEOWNERS = ["@shelson"]
DEPENDENCIES = ["esp32"]
AUTO_LOAD = ["json", "sensor", "text_sensor", "binary_sensor", "esp32_ble_server"]
MULTI_CONF = False

CONF_DEVICE_NAME = "device_name"
CONF_LIVENESS_TIMEOUT = "liveness_timeout"
CONF_BLE_SERVER_ID = "ble_server_id"
CONF_LINE_MAX = "line_max"
CONF_TX_QUEUE_DEPTH = "tx_queue_depth"
CONF_LVGL_WIDGETS = "lvgl_widgets"
CONF_UI_CONTAINER = "ui_container"
CONF_ON_TURN = "on_turn"
CONF_ON_PERMISSION = "on_permission"
CONF_ON_LIVENESS = "on_liveness"
CONF_ON_COMMAND = "on_command"

ai_desktop_buddy_ns = cg.esphome_ns.namespace("ai_desktop_buddy")
AIDesktopBuddy = ai_desktop_buddy_ns.class_("AIDesktopBuddy", cg.Component)

AIDesktopBuddyApproveAction = ai_desktop_buddy_ns.class_(
    "AIDesktopBuddyApproveAction", automation.Action
)
AIDesktopBuddyDenyAction = ai_desktop_buddy_ns.class_(
    "AIDesktopBuddyDenyAction", automation.Action
)

LVGL_WIDGETS_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_UI_CONTAINER): cv.use_id(lv_obj_t),
    }
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(AIDesktopBuddy),
        cv.Optional(CONF_DEVICE_NAME, default="Claude Buddy"): cv.string,
        cv.Optional(
            CONF_LIVENESS_TIMEOUT, default="30s"
        ): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_LINE_MAX, default=2048): cv.int_range(min=256, max=4096),
        cv.Optional(CONF_TX_QUEUE_DEPTH, default=8): cv.int_range(min=2, max=32),
        cv.Optional(CONF_LVGL_WIDGETS): LVGL_WIDGETS_SCHEMA,
        cv.Optional(CONF_ON_TURN): automation.validate_automation({}),
        cv.Optional(CONF_ON_PERMISSION): automation.validate_automation({}),
        cv.Optional(CONF_ON_LIVENESS): automation.validate_automation({}),
        cv.Optional(CONF_ON_COMMAND): automation.validate_automation({}),
    }
).extend(cv.COMPONENT_SCHEMA)


def _make_entity_config(entity_type, entity_name):
    return {
        CONF_ID: ID(entity_name, is_declaration=True, type=entity_type),
        CONF_NAME: entity_name,
        CONF_DISABLED_BY_DEFAULT: False,
    }


async def to_code(config):
    # Register the loggers this component needs
    esp32_ble.register_bt_logger(BTLoggers.GATT, BTLoggers.SMP)
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_device_name(config[CONF_DEVICE_NAME]))
    cg.add(
        var.set_liveness_timeout_ms(config[CONF_LIVENESS_TIMEOUT].total_milliseconds)
    )

    cg.add_define("AI_BUDDY_LINE_MAX", config[CONF_LINE_MAX])
    cg.add_define("AI_BUDDY_TX_QUEUE_DEPTH", config[CONF_TX_QUEUE_DEPTH])
    cg.add_define("ESPHOME_ESP32_BLE_GAP_EVENT_HANDLER_COUNT", 1)

    # --- Numeric sensors ---
    for sensor_name, setter_name in [
        ("total_tasks", "set_total_sensor"),
        ("running_tasks", "set_running_sensor"),
        ("waiting_tasks", "set_waiting_sensor"),
        ("tokens", "set_tokens_sensor"),
        ("tokens_today", "set_tokens_today_sensor"),
    ]:
        sconfig = _make_entity_config(sensor.Sensor, sensor_name)
        sconfig[CONF_FORCE_UPDATE] = False
        svar = await sensor.new_sensor(sconfig)
        cg.add(getattr(var, setter_name)(svar))

    # --- Text sensors ---
    for sensor_name, setter_name in [
        ("status_message", "set_status_text_sensor"),
        ("prompt_data", "set_prompt_text_sensor"),
        ("entries_data", "set_entries_text_sensor"),
        ("passkey", "set_passkey_text_sensor"),
    ]:
        tsconfig = _make_entity_config(text_sensor.TextSensor, sensor_name)
        tsvar = await text_sensor.new_text_sensor(tsconfig)
        cg.add(getattr(var, setter_name)(tsvar))

    # --- Binary sensor ---
    bsconfig = _make_entity_config(binary_sensor.BinarySensor, "liveness")
    bsvar = await binary_sensor.new_binary_sensor(bsconfig)
    cg.add(var.set_liveness_binary_sensor(bsvar))

    # LVGL widget bindings
    if CONF_LVGL_WIDGETS in config:
        add_lv_use("button")
        widgets = config[CONF_LVGL_WIDGETS]
        w = await cg.get_variable(widgets[CONF_UI_CONTAINER])
        cg.add(var.set_lvgl_ui_container(w))

    # Automations via build_callback_automation
    for conf_key, method_name, args in [
        (
            CONF_ON_TURN,
            "add_on_turn_callback",
            [(cg.std_string, "role"), (cg.std_string, "content")],
        ),
        (
            CONF_ON_PERMISSION,
            "add_on_permission_callback",
            [(cg.std_string, "id"), (cg.std_string, "tool"), (cg.std_string, "hint")],
        ),
        (
            CONF_ON_LIVENESS,
            "add_on_liveness_callback",
            [(cg.bool_, "live")],
        ),
        (
            CONF_ON_COMMAND,
            "add_on_command_callback",
            [(cg.std_string, "cmd"), (cg.std_string, "data")],
        ),
    ]:
        for conf in config.get(conf_key, []):
            await automation.build_callback_automation(var, method_name, args, conf)


@automation.register_action(
    "ai_desktop_buddy.approve",
    AIDesktopBuddyApproveAction,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(AIDesktopBuddy),
        }
    ),
    synchronous=True,
)
async def buddy_approve_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


@automation.register_action(
    "ai_desktop_buddy.deny",
    AIDesktopBuddyDenyAction,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(AIDesktopBuddy),
        }
    ),
    synchronous=True,
)
async def buddy_deny_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)
