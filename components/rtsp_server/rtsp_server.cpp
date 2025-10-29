#include "rtsp_server.h"
#include "esphome/core/log.h"
#include <cstring>

namespace esphome {
namespace rtsp_server {

static const char *TAG = "rtsp_server";

void RTSPServer::setup() {
  ESP_LOGI(TAG, "🎬 Starting RTSP server on port %u", this->port_);
  
  if (!this->encoder_ || !this->encoder_->is_initialized()) {
    ESP_LOGE(TAG, "❌ H.264 encoder not ready");
    this->mark_failed();
    return;
  }
  
  if (!this->start_server_()) {
    ESP_LOGE(TAG, "❌ Failed to start RTSP server");
    this->mark_failed();
    return;
  }
  
  ESP_LOGI(TAG, "✅ RTSP server ready at rtsp://<IP>:%u%s", 
           this->port_, this->path_.c_str());
}

bool RTSPServer::start_server_() {
  this->server_socket_ = socket(AF_INET, SOCK_STREAM, 0);
  if (this->server_socket_ < 0) {
    return false;
  }
  
  int opt = 1;
  setsockopt(this->server_socket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
  
  struct sockaddr_in addr = {};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(this->port_);
  addr.sin_addr.s_addr = INADDR_ANY;
  
  if (bind(this->server_socket_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
    close(this->server_socket_);
    return false;
  }
  
  if (listen(this->server_socket_, 2) < 0) {
    close(this->server_socket_);
    return false;
  }
  
  // Mode non-bloquant
  fcntl(this->server_socket_, F_SETFL, O_NONBLOCK);
  
  return true;
}

void RTSPServer::loop() {
  if (this->server_socket_ < 0) return;
  
  struct sockaddr_in client_addr;
  socklen_t addr_len = sizeof(client_addr);
  
  int client_sock = accept(this->server_socket_, 
                            (struct sockaddr*)&client_addr, &addr_len);
  
  if (client_sock >= 0) {
    ESP_LOGI(TAG, "📥 Client connected");
    this->handle_client_(client_sock);
    close(client_sock);
  }
}

void RTSPServer::handle_client_(int sock) {
  char buffer[1024];
  int n = recv(sock, buffer, sizeof(buffer) - 1, 0);
  
  if (n <= 0) return;
  buffer[n] = '\0';
  
  // Parser la requête RTSP basique
  if (strstr(buffer, "DESCRIBE")) {
    const char *sdp = 
      "v=0\r\n"
      "s=ESPHome Camera Stream\r\n"
      "m=video 0 RTP/AVP 96\r\n"
      "a=rtpmap:96 H264/90000\r\n"
      "a=control:track1\r\n";
    
    this->send_rtsp_response_(sock, "200 OK", sdp);
    
  } else if (strstr(buffer, "SETUP")) {
    const char *transport = "RTP/AVP;unicast;client_port=5000-5001\r\n";
    this->send_rtsp_response_(sock, "200 OK", transport);
    
  } else if (strstr(buffer, "PLAY")) {
    this->send_rtsp_response_(sock, "200 OK", "");
    this->stream_h264_(sock);
  }
}

void RTSPServer::send_rtsp_response_(int sock, const char *status, 
                                      const char *content) {
  char response[2048];
  snprintf(response, sizeof(response),
           "RTSP/1.0 %s\r\n"
           "CSeq: 1\r\n"
           "Content-Length: %d\r\n"
           "\r\n%s",
           status, (int)strlen(content), content);
  
  send(sock, response, strlen(response), 0);
}

void RTSPServer::stream_h264_(int sock) {
  ESP_LOGI(TAG, "🎥 Starting H.264 stream");
  
  uint8_t *h264_data = nullptr;
  size_t h264_size = 0;
  bool is_keyframe = false;
  
  for (int i = 0; i < 300; i++) {  // Stream 300 frames (~10s @ 30fps)
    // Obtenir une frame RGB depuis la caméra
    uint8_t *camera_data = /* obtenir depuis caméra */;
    size_t camera_size = /* taille frame */;
    
    // Encoder en H.264
    esp_err_t ret = this->encoder_->encode_frame(
      camera_data, camera_size,
      &h264_data, &h264_size, &is_keyframe
    );
    
    if (ret == ESP_OK && h264_size > 0) {
      // Envoyer via RTP (simplifié ici)
      send(sock, h264_data, h264_size, 0);
      
      ESP_LOGD(TAG, "📤 Sent %s frame (%u bytes)", 
               is_keyframe ? "I" : "P", (unsigned)h264_size);
    }
    
    delay(33);  // ~30 FPS
  }
  
  ESP_LOGI(TAG, "✅ Stream finished");
}

} // namespace rtsp_server
} // namespace esphome
