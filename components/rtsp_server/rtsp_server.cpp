#include "rtsp_server.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include <cerrno>
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
@@ -93,57 +95,97 @@ void RTSPServer::handle_client_(int sock) {
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
  

  if (this->encoder_ == nullptr || !this->encoder_->is_initialized()) {
    ESP_LOGE(TAG, "Encoder not ready, aborting stream");
    return;
  }

  auto *camera = this->encoder_->get_camera();
  if (camera == nullptr) {
    ESP_LOGE(TAG, "No camera attached to encoder");
    return;
  }

  if (!camera->is_streaming() && !camera->start_streaming()) {
    ESP_LOGE(TAG, "Unable to start camera stream");
    return;
  }

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
  uint32_t last_sequence = camera->get_current_sequence();

  while (true) {
    if (!camera->acquire_frame(last_sequence)) {
      esphome::delay(5);
      continue;
    }
    
    delay(33);  // ~30 FPS

    uint8_t *camera_data = camera->get_image_data();
    size_t camera_size = camera->get_image_size();
    uint32_t current_sequence = camera->get_current_sequence();

    if (camera_data == nullptr || camera_size == 0) {
      ESP_LOGW(TAG, "Camera returned empty frame");
      camera->release_frame();
      esphome::delay(5);
      continue;
    }

    esp_err_t ret = this->encoder_->encode_frame(camera_data, camera_size,
                                                 &h264_data, &h264_size, &is_keyframe);
    camera->release_frame();

    if (ret != ESP_OK || h264_size == 0) {
      ESP_LOGW(TAG, "Failed to encode frame: err=0x%x size=%u", ret, (unsigned)h264_size);
      esphome::delay(5);
      continue;
    }

    ssize_t sent = send(sock, h264_data, h264_size, 0);
    this->encoder_->release_output_buffer(h264_data);

    if (sent < 0) {
      ESP_LOGE(TAG, "Socket send failed: errno=%d", errno);
      break;
    }

    if (static_cast<size_t>(sent) != h264_size) {
      ESP_LOGW(TAG, "Partial frame sent: %d/%u bytes", (int)sent, (unsigned)h264_size);
    }

    ESP_LOGD(TAG, "📤 Sent %s frame (%u bytes)", is_keyframe ? "I" : "P", (unsigned)h264_size);

    last_sequence = current_sequence;
    esphome::delay(33);
  }
  

  ESP_LOGI(TAG, "✅ Stream finished");
}

} // namespace rtsp_server
} // namespace esphome
