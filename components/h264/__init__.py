import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = ["@youkorr"]
DEPENDENCIES = ["mipi_dsi_cam"]

h264_ns = cg.esphome_ns.namespace("h264")
H264Encoder = h264_ns.class_("H264Encoder", cg.Component)

# Ajouter la référence à la caméra
CONF_CAMERA_ID = "camera_id"
CONF_BITRATE = "bitrate"
CONF_GOP_SIZE = "gop_size"

# Import du namespace mipi_dsi_cam pour la référence
mipi_dsi_cam_ns = cg.esphome_ns.namespace("mipi_dsi_cam")
MipiDsiCam = mipi_dsi_cam_ns.class_("MipiDsiCam")

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(H264Encoder),
    cv.Required(CONF_CAMERA_ID): cv.use_id(MipiDsiCam),  # ✅ AJOUTER
    cv.Optional(CONF_BITRATE, default=2000000): cv.int_range(min=100000, max=20000000),
    cv.Optional(CONF_GOP_SIZE, default=30): cv.int_range(min=1, max=300),
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    
    # Lier à la caméra ✅
    camera = await cg.get_variable(config[CONF_CAMERA_ID])
    cg.add(var.set_camera(camera))
    
    cg.add(var.set_bitrate(config[CONF_BITRATE]))
    cg.add(var.set_gop_size(config[CONF_GOP_SIZE]))
    cg.add_define("USE_H264_ENCODER")
