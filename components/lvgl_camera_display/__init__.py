import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

DEPENDENCIES = ["lvgl", "mipi_dsi_cam"]
AUTO_LOAD = ["mipi_dsi_cam"]

CONF_CAMERA_ID = "camera_id"
CONF_CANVAS_ID = "canvas_id"
CONF_UPDATE_INTERVAL = "update_interval"
CONF_ROTATION = "rotation"
CONF_MIRROR_X = "mirror_x"
CONF_MIRROR_Y = "mirror_y"

lvgl_camera_display_ns = cg.esphome_ns.namespace("lvgl_camera_display")
LVGLCameraDisplay = lvgl_camera_display_ns.class_("LVGLCameraDisplay", cg.Component)

RotationAngle = lvgl_camera_display_ns.enum("RotationAngle")
Rotation = lvgl_camera_display_ns.enum("Rotation")
ROTATION_ANGLES = {
    0: RotationAngle.ROTATION_0,
    90: RotationAngle.ROTATION_90,
    180: RotationAngle.ROTATION_180,
    270: RotationAngle.ROTATION_270,
    0: Rotation.DEG_0,
    90: Rotation.DEG_90,
    180: Rotation.DEG_180,
    270: Rotation.DEG_270,
}

# Import du namespace mipi_dsi_cam
mipi_dsi_cam_ns = cg.esphome_ns.namespace("mipi_dsi_cam")
MipiDsiCam = mipi_dsi_cam_ns.class_("MipiDsiCam")

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(LVGLCameraDisplay),
    cv.Required(CONF_CAMERA_ID): cv.use_id(MipiDsiCam),
    cv.Required(CONF_CANVAS_ID): cv.string,
    cv.Optional(CONF_UPDATE_INTERVAL, default="33ms"): cv.positive_time_period_milliseconds,
    cv.Optional(CONF_ROTATION, default=0): cv.enum(ROTATION_ANGLES, int=True),
    cv.Optional(CONF_MIRROR_X, default=False): cv.boolean,
    cv.Optional(CONF_MIRROR_Y, default=False): cv.boolean,
}).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    
    # Lier à la caméra
    camera = await cg.get_variable(config[CONF_CAMERA_ID])
    cg.add(var.set_camera(camera))
