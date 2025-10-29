import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = ["@youkorr"]
DEPENDENCIES = ["mipi_dsi_cam"] #["mipi_dsi_cam"]

jpeg_ns = cg.esphome_ns.namespace("jpeg")
JPEGEncoder = jpeg_ns.class_("JPEGEncoder", cg.Component)

CONF_CAMERA_ID = "camera_id"
CONF_QUALITY = "quality"

# Import du namespace mipi_dsi_cam
mipi_dsi_cam_ns = cg.esphome_ns.namespace("mipi_dsi_cam")
MipiDsiCam = mipi_dsi_cam_ns.class_("MipiDsiCam")

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(JPEGEncoder),
    cv.Required(CONF_CAMERA_ID): cv.use_id(MipiDsiCam),  # ✅ AJOUTER
    cv.Optional(CONF_QUALITY, default=80): cv.int_range(min=1, max=100),
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    
    # Lier à la caméra ✅
    camera = await cg.get_variable(config[CONF_CAMERA_ID])
    cg.add(var.set_camera(camera))
    
    cg.add(var.set_quality(config[CONF_QUALITY]))
    cg.add_define("USE_JPEG_ENCODER")
