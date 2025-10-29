#pragma once
#include "esphome/core/component.h"
#include "../h264/h264_encoder.h"
#include <lwip/sockets.h>

namespace esphome {
namespace rtsp_server {

class RTSPServer : public Component {
 public:
  void setup() override;
  void loop() override;
  
  void set_encoder(h264::H264Encoder *encoder) { encoder_ = encoder; }
  void set_port(uint16_t port) { port_ = port; }
  void set_path(const std::string &path) { path_ = path; }
  
 protected:
  h264::H264Encoder *encoder_{nullptr};
  uint16_t port_{8554};
  std::string path_{"/stream"};
  int server_socket_{-1};
  
  bool start_server_();
  void handle_client_(int client_sock);
  void send_rtsp_response_(int sock, const char *status, const char *content);
  void stream_h264_(int sock);
};

} // namespace rtsp_server
} // namespace esphome
