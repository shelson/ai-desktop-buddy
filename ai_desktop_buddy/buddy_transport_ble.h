#pragma once

#include <cstdint>
#include <string>
#include <esp_gap_ble_api.h>
#include "esphome/components/esp32_ble_server/ble_server.h"
#include "esphome/components/esp32_ble_server/ble_characteristic.h"
#include "esphome/components/esp32_ble_server/ble_2902.h"
#include "esphome/components/esp32_ble/ble.h"
#include "esphome/components/bytebuffer/bytebuffer.h"
#include "esphome/core/component.h"
#include "esp_mac.h"
#include "buddy_protocol.h"

namespace esphome {
namespace ai_desktop_buddy {

using namespace esp32_ble_server;

class BuddyTransportBLE : public Parented<esp32_ble::ESP32BLE> {
 public:
  // float get_setup_priority() const override { return setup_priority::AFTER_BLUETOOTH + 20; }
  void setup(BuddyProtocol *protocol);
  void loop();
  bool create_service();

  void set_device_name(const std::string &name) { this->device_name_ = build_default_name(); }
  const std::string &get_device_name() const { return this->device_name_; }

  bool is_tx_ready() const;
  void set_passkey_callback(std::function<void(uint32_t)> &&cb) { this->passkey_callback_ = std::move(cb); }

 protected:
  void handle_rx_write_(std::span<const uint8_t> data, uint16_t conn_id);
  void process_tx_queue_();
  void start_custom_advertising_();
  void handle_gap_security_event_(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
  std::string build_default_name(void);

  BuddyProtocol *protocol_{nullptr};
  std::string device_name_;
  BLEService *service_{nullptr};
  BLECharacteristic *rx_char_{nullptr};
  BLECharacteristic *tx_char_{nullptr};
  bool subscribed_{false};
  bool connected_{false};
  bool service_created_{false};
  bool service_started_{false};
  uint32_t retry_ms_{0};
  uint8_t adv_uuid_[16]{};
  TxFrame tx_frame_buf_{};
  bool encrypted_{false};
  std::function<void(uint32_t)> passkey_callback_;
};

}  // namespace ai_desktop_buddy
}  // namespace esphome
