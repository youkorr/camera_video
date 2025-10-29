import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

DEPENDENCIES = ["h264"]

rtsp_server_ns = cg.esphome_ns.namespace("rtsp_server")
RTSPServer = rtsp_server_ns.class_("RTSPServer", cg.Component)

CONF_H264_ENCODER_ID = "h264_encoder_id"
CONF_PORT = "port"
CONF_PATH = "path"

h264_ns = cg.esphome_ns.namespace("h264")
H264Encoder = h264_ns.class_("H264Encoder")

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(RTSPServer),
    cv.Required(CONF_H264_ENCODER_ID): cv.use_id(H264Encoder),
    cv.Optional(CONF_PORT, default=8554): cv.port,
    cv.Optional(CONF_PATH, default="/stream"): cv.string,
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    
    h264 = await cg.get_variable(config[CONF_H264_ENCODER_ID])
    cg.add(var.set_encoder(h264))
    cg.add(var.set_port(config[CONF_PORT]))
    cg.add(var.set_path(config[CONF_PATH]))
