import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import esp32_camera
from esphome.const import (
    CONF_ID,
    CONF_ROTATION,
)
from esphome.core import CORE

DEPENDENCIES = ["esp32_camera", "lvgl"]
AUTO_LOAD = ["esp32_camera"]

CONF_CAMERA_ID = "camera_id"
CONF_UPDATE_INTERVAL = "update_interval"
CONF_DIRECT_MODE = "direct_mode"
CONF_USE_PPA = "use_ppa"
CONF_MIRROR_X = "mirror_x"
CONF_MIRROR_Y = "mirror_y"

lvgl_camera_display_ns = cg.esphome_ns.namespace("lvgl_camera_display")
LVGLCameraDisplay = lvgl_camera_display_ns.class_("LVGLCameraDisplay", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(LVGLCameraDisplay),
        cv.Required(CONF_CAMERA_ID): cv.use_id(esp32_camera.ESP32Camera),
        cv.Optional(CONF_UPDATE_INTERVAL, default="33ms"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_DIRECT_MODE, default=True): cv.boolean,
        cv.Optional(CONF_USE_PPA, default=True): cv.boolean,
        cv.Optional(CONF_ROTATION, default=0): cv.int_range(min=0, max=270, step=90),
        cv.Optional(CONF_MIRROR_X, default=False): cv.boolean,
        cv.Optional(CONF_MIRROR_Y, default=True): cv.boolean,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    camera = await cg.get_variable(config[CONF_CAMERA_ID])
    cg.add(var.set_camera(camera))
    
    cg.add(var.set_update_interval(config[CONF_UPDATE_INTERVAL]))
    cg.add(var.set_direct_mode(config[CONF_DIRECT_MODE]))
    cg.add(var.set_use_ppa(config[CONF_USE_PPA]))
    cg.add(var.set_rotation(config[CONF_ROTATION]))
    cg.add(var.set_mirror_x(config[CONF_MIRROR_X]))
    cg.add(var.set_mirror_y(config[CONF_MIRROR_Y]))

    # Inclure le header
    cg.add_library("lvgl/lvgl", "8.3.11")
    
    # Ajouter les defines nécessaires pour le PPA
    if config[CONF_USE_PPA]:
        cg.add_define("USE_PPA_ACCELERATION")

