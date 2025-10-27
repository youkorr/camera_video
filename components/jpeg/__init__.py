import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = ["@youkorr"]
DEPENDENCIES = ["mipi_dsi_cam"]

jpeg_ns = cg.esphome_ns.namespace("jpeg")
JPEGEncoder = jpeg_ns.class_("JPEGEncoder", cg.Component)

CONF_QUALITY = "quality"

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(JPEGEncoder),
    cv.Optional(CONF_QUALITY, default=80): cv.int_range(min=1, max=100),
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_quality(config[CONF_QUALITY]))
    cg.add_define("USE_JPEG_ENCODER")
    # ...
