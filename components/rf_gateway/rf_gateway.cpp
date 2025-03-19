#include "rf_gateway.h"

void RFGateway::setup() {
  ESP_LOGD("rf_gateway", "RF网关初始化");

  if (id(mqtt_client)->is_connected()) {
    id(mqtt_client)->publish_json("esp-rf/debug", [](JsonObject root) {
      root["type"] = "system";
      root["event"] = "startup";
      root["timestamp"] = millis();
      root["version"] = "1.1";

      JsonObject info = root.createNestedObject("info");
      info["device"] = "RF Gateway";
      info["status"] = "online";
      info["uptime"] = millis();
    });
  }
}

void RFGateway::send_rf_data(const std::string &payload) {
  if (payload.length() != 16) {
    ESP_LOGW("rf_gateway", "无效的发送数据长度: %d", payload.length());
    id(mqtt_client)->publish_json("esp-rf/debug", [payload](JsonObject root) {
      root["type"] = "error";
      root["event"] = "send_failed";
      root["timestamp"] = millis();
      root["reason"] = "invalid_length";
      root["data"] = payload;
    });
    return;
  }

  if (payload.find_first_not_of("0123456789ABCDEFabcdef") != std::string::npos) {
    ESP_LOGW("rf_gateway", "无效的十六进制格式: %s", payload.c_str());
    id(mqtt_client)->publish_json("esp-rf/debug", [payload](JsonObject root) {
      root["type"] = "error";
      root["event"] = "send_failed";
      root["timestamp"] = millis();
      root["reason"] = "invalid_hex";
      root["data"] = payload;
    });
    return;
  }

  std::vector<uint8_t> packet;
  packet.reserve(8);
  bool parse_error = false;

  for (size_t i = 0; i < payload.length() && !parse_error; i += 2) {
    std::string byte_str = payload.substr(i, 2);
    char* end_ptr;
    unsigned long value = strtoul(byte_str.c_str(), &end_ptr, 16);

    if (*end_ptr != '\0' || value > 0xFF) {
      parse_error = true;
      break;
    }

    packet.push_back(static_cast<uint8_t>(value));
  }

  if (parse_error) {
    ESP_LOGW("rf_gateway", "解析数据失败: %s", payload.c_str());
    id(mqtt_client)->publish_json("esp-rf/debug", [payload](JsonObject root) {
      root["type"] = "error";
      root["event"] = "send_failed";
      root["timestamp"] = millis();
      root["reason"] = "parse_error";
      root["data"] = payload;
    });
    return;
  }

  if (packet[0] != 0xFD || packet[7] != 0xDF) {
    ESP_LOGW("rf_gateway", "无效的包格式: %s", payload.c_str());
    id(mqtt_client)->publish_json("esp-rf/debug", [payload](JsonObject root) {
      root["type"] = "error";
      root["event"] = "send_failed";
      root["timestamp"] = millis();
      root["reason"] = "invalid_packet";
      root["data"] = payload;
    });
    return;
  }

  id(uart_bus).write_array(packet.data(), packet.size());

  id(mqtt_client)->publish_json("esp-rf/debug", [payload](JsonObject root) {
    root["type"] = "sent";
    root["event"] = "send_success";
    root["timestamp"] = millis();
    root["data"] = payload;
  });

  ESP_LOGD("rf_gateway", "成功发送RF数据: %s", payload.c_str());
}
