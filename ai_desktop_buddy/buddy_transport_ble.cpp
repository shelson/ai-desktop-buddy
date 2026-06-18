#include <string>
#include <esp_gap_ble_api.h>
#include <esp_gatts_api.h>
#include "buddy_transport_ble.h"
#include "esphome/core/log.h"
#include "esphome/components/esp32_ble/ble.h"
#include "esp_timer.h"

// #include "services/gatt/ble_svc_gatt.h"
// #include "store/config/ble_store_config.h"

// Weak reference to global_ble_server — set by BLEServer::setup() when
// // esp32_ble_server C++ code is compiled. Stays nullptr otherwise.
// namespace esphome {
// namespace esp32_ble_server {
// __attribute__((weak)) BLEServer *global_ble_server = nullptr;
// }  // namespace esp32_ble_server
// }  // namespace esphome

namespace esphome {
namespace ai_desktop_buddy {

static const char *const TAG = "buddy_transport_ble";

static const ESPBTUUID BUDDY_NUS_SERVICE_UUID =
    ESPBTUUID::from_raw((const uint8_t[]) {0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
                     0x93, 0xf3, 0xa3, 0xb5, 0x01, 0x00, 0x40, 0x6e});
static const ESPBTUUID BUDDY_NUS_RX_UUID =
    ESPBTUUID::from_raw((const uint8_t[]) {0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
                     0x93, 0xf3, 0xa3, 0xb5, 0x02, 0x00, 0x40, 0x6e});
static const ESPBTUUID BUDDY_NUS_TX_UUID =
    ESPBTUUID::from_raw((const uint8_t[]) {0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
                     0x93, 0xf3, 0xa3, 0xb5, 0x03, 0x00, 0x40, 0x6e});

// static const uint8_t *const RX_CHAR_UUID = (const uint8_t *)"6e400002-b5a3-f393-e0a9-e50e24dcca9e";
// static const uint8_t *const TX_CHAR_UUID = (const uint8_t *)"6e400003-b5a3-f393-e0a9-e50e24dcca9e";

// static const ESPBTUUID SERVICE_UUID = ESPBTUUID::from_raw(SERVICE_UUID_STR);
// static const ESPBTUUID NUS_TX_UUID = ESPBTUUID::from_raw(NUS_TX_UUID_BYTES);
// static const ESPBTUUID NUS_RX_UUID = ESPBTUUID::from_raw(NUS_RX_UUID_BYTES);
// static const ESPBTUUID BUDDY_NUS_RX_UUID =
//     ESPBTUUID::from_uint16(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
//                      0x93, 0xf3, 0xa3, 0xb5, 0x02, 0x00, 0x40, 0x6e);
// static const ESPBTUUID BUDDY_NUS_TX_UUID =
//     ESPBTUUID::from_uint16(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
//                      0x93, 0xf3, 0xa3, 0xb5, 0x03, 0x00, 0x40, 0x6e);

void dump_adv_data(const char* label, const esp_ble_adv_data_t* adv) {
    if (adv == nullptr) {
        ESP_LOGI(TAG, "[%s] null adv data", label);
        return;
    }

    ESP_LOGI(TAG, "=== ADV DATA DUMP: %s ===", label);
    
    int total = 0;

    // Fixed fields
    ESP_LOGI(TAG, "  include_name:     %s", adv->include_name ? "yes" : "no");
    ESP_LOGI(TAG, "  include_txpower:  %s", adv->include_txpower ? "yes" : "no");
    ESP_LOGI(TAG, "  appearance:       0x%04x", adv->appearance);
    ESP_LOGI(TAG, "  min_interval:     %d (%.2f ms)", adv->min_interval, adv->min_interval * 1.25f);
    ESP_LOGI(TAG, "  max_interval:     %d (%.2f ms)", adv->max_interval, adv->max_interval * 1.25f);
    ESP_LOGI(TAG, "  flag:             0x%02x", adv->flag);

    // Flag field in packet
    if (adv->flag) {
        ESP_LOGI(TAG, "  [packet] flags: 3 bytes");
        total += 3;
    }

    // TX power
    if (adv->include_txpower) {
        ESP_LOGI(TAG, "  [packet] tx power: 3 bytes");
        total += 3;
    }

    // Appearance
    if (adv->appearance) {
        ESP_LOGI(TAG, "  [packet] appearance: 4 bytes");
        total += 4;
    }

    // Slave connection interval
    if (adv->min_interval || adv->max_interval) {
        ESP_LOGI(TAG, "  [packet] conn interval: 6 bytes");
        total += 6;
    }

    // Manufacturer data
    if (adv->manufacturer_len > 0 && adv->p_manufacturer_data != nullptr) {
        int field_len = 2 + adv->manufacturer_len;  // type + len overhead
        ESP_LOGI(TAG, "  [packet] manufacturer data: %d bytes (payload: %d)", field_len, adv->manufacturer_len);
        // Hex dump
        char hex[adv->manufacturer_len * 3 + 1];
        for (int i = 0; i < adv->manufacturer_len; i++) {
            sprintf(hex + i * 3, "%02x ", adv->p_manufacturer_data[i]);
        }
        ESP_LOGI(TAG, "    data: %s", hex);
        total += field_len;
    }

    // Service data
    if (adv->service_data_len > 0 && adv->p_service_data != nullptr) {
        int field_len = 2 + adv->service_data_len;
        ESP_LOGI(TAG, "  [packet] service data: %d bytes (payload: %d)", field_len, adv->service_data_len);
        total += field_len;
    }

    // Service UUIDs
    if (adv->service_uuid_len > 0 && adv->p_service_uuid != nullptr) {
        int field_len = 2 + adv->service_uuid_len;
        ESP_LOGI(TAG, "  [packet] service uuid(s): %d bytes (payload: %d)", field_len, adv->service_uuid_len);
        // Dump each UUID (assuming 128-bit = 16 bytes each)
        if (adv->service_uuid_len % 16 == 0) {
            int count = adv->service_uuid_len / 16;
            for (int i = 0; i < count; i++) {
                const uint8_t* u = adv->p_service_uuid + (i * 16);
                // Print in reversed byte order (little-endian -> UUID string)
                ESP_LOGI(TAG, "    uuid[%d]: %02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                    i,
                    // u[0],u[1],u[2],u[3],u[4],
                    // u[5],u[6],
                    // u[7],u[8],
                    // u[9],u[10],
                    // u[11],u[12],u[13],u[14],u[15]);
                    u[15],u[14],u[13],u[12],
                    u[11],u[10],
                    u[9],u[8],
                    u[7],u[6],
                    u[5],u[4],u[3],u[2],u[1],u[0]);
            }
        } else {
            // Just hex dump if not 128-bit aligned
            char hex[adv->service_uuid_len * 3 + 1];
            for (int i = 0; i < adv->service_uuid_len; i++) {
                sprintf(hex + i * 3, "%02x ", adv->p_service_uuid[i]);
            }
            ESP_LOGI(TAG, "    raw: %s", hex);
        }
        total += field_len;
    }

    // Device name (estimated - actual name length not in struct)
    if (adv->include_name) {
        total += 9;
        ESP_LOGI(TAG, "  [packet] device name: 10 bytes (Claude-48)");
    }

    ESP_LOGI(TAG, "  [packet] TOTAL : %d / 31 bytes", total);
    ESP_LOGI(TAG, "=========================================");
}

std::string BuddyTransportBLE::build_default_name(void)
{
    uint8_t mac[6] = {0};

    if (esp_read_mac(mac, ESP_MAC_BT) != ESP_OK) {
        esp_read_mac(mac, ESP_MAC_WIFI_STA);
    }
    uint8_t out_size = 10;
    char out[out_size];
    snprintf(out, out_size, "Claude-%02X%02X", mac[4], mac[5]);
    return(std::string(out));
}

void BuddyTransportBLE::setup(BuddyProtocol *protocol) {
  this->protocol_ = protocol;
  memcpy(this->adv_uuid_,
         BUDDY_NUS_SERVICE_UUID.as_128bit().get_uuid().uuid.uuid128,
         16);

  esp32_ble::global_ble->add_gap_event_callback([this](esp_gap_ble_cb_event_t event,
                                                        esp_ble_gap_cb_param_t *param) {
    this->handle_gap_security_event_(event, param);
  });
}

void BuddyTransportBLE::loop() {
  if (!global_ble_server->is_running()) {
    ESP_LOGD(TAG, "No global BLE Server yet");
    return;
  }
  if (this->service_ == nullptr) {
    this->create_service();
    return;
  }

  if (this->service_->is_created()) {
    // Try to start it
    ESP_LOGD(TAG, "Trying to start server");
    this->service_->start();
    return;
  }

  if (this->service_->is_starting()) {
    // this is okay, we just wait a bit
    return;
  }
  if (this->service_->is_running()) {
    if(!this->service_started_) {
      // We are alive and need to set our flag as we only want to run the below once.
      this->service_started_ = true;
      ESP_LOGI(TAG, "NUS BLE service running, starting advertising...");

      global_ble_server->on_connect([this](uint16_t conn_id) {
        ESP_LOGI(TAG, "Client connected — conn_id=%d", conn_id);
        this->connected_ = true;
        this->subscribed_ = false;
        this->encrypted_ = false;
      });
      global_ble_server->on_disconnect([this](uint16_t conn_id) {
        ESP_LOGI(TAG, "Client disconnected — conn_id=%d", conn_id);
        this->connected_ = false;
        this->subscribed_ = false;
        this->encrypted_ = false;
      });

      esp32_ble::global_ble->advertising_register_raw_advertisement_callback(
          [this](bool started) {
            if (!started)
              return;
            this->start_custom_advertising_();
          });

      ESP_LOGI(TAG, "  Advertising as: '%s'", this->device_name_.c_str());
#ifndef USE_ESP32_BLE_ADVERTISING
      ESP_LOGW(TAG, "  WARNING: esp32_ble advertising is not enabled!");
      ESP_LOGW(TAG, "  Add 'advertising: true' under 'esp32_ble:' in your YAML config.");
#endif
      return;
    }
  }
  this->process_tx_queue_();
}

bool BuddyTransportBLE::create_service() {
  if (this->service_ != nullptr)
    return true;

  if (!this->retry_ms_) {
    this->retry_ms_ = millis();
    ESP_LOGI(TAG, "Waiting for BLE server... (device_name='%s')", this->device_name_.c_str());
    return false;
  }

  if (millis() - this->retry_ms_ < 5000) {
    return false;
  }
  this->retry_ms_ = 0;

  if (!global_ble_server->is_running()) {
    ESP_LOGW(TAG, "BLE server not yet running. Retrying...");
    this->retry_ms_ = millis();
    return false;
  }
  ESP_LOGD(TAG, "Global BLE Server now available, adding service and characteristics");
  //esp32_ble::global_ble->gap_set_device_name(this->get_device_name());

  this->service_ = global_ble_server->create_service(BUDDY_NUS_SERVICE_UUID, false, 8);
  if (this->service_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create NUS service");
    return false;
  }
  ESP_LOGD(TAG, "Created service successfully");
  ESP_LOGD(TAG, "Service instance ID: %d", this->service_->get_inst_id());

  this->rx_char_ = this->service_->create_characteristic(
      BUDDY_NUS_RX_UUID,
      BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);

  if (this->rx_char_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create RX characteristic");
    return false;
  }
  ESP_LOGD(TAG, "Created RX Characteristic");

  this->rx_char_->on_write([this](std::span<const uint8_t> data, uint16_t conn_id) {
    this->handle_rx_write_(data, conn_id);
  });

  this->tx_char_ = this->service_->create_characteristic(
      BUDDY_NUS_TX_UUID,
      BLECharacteristic::PROPERTY_NOTIFY);

  if (this->tx_char_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create TX characteristic");
    return false;
  }
  ESP_LOGD(TAG, "Created TX Characteristic");

  BLEDescriptor *cccd = new BLE2902();
  this->tx_char_->add_descriptor(cccd);

  //  BLEDescriptor *rxdesc = new BLE2902();
  // this->rx_char_->add_descriptor(rxdesc); 
  // this->service_->start();

  ESP_LOGI(TAG, "NUS BLE service created, waiting for BLE stack to start it...");

  return true;
}

void BuddyTransportBLE::handle_rx_write_(std::span<const uint8_t> data, uint16_t conn_id) {
  if (this->protocol_ != nullptr && !data.empty()) {
    this->protocol_->receive_bytes(data.data(), data.size());
  }
}

void BuddyTransportBLE::process_tx_queue_() {
  if (this->protocol_ == nullptr || this->tx_char_ == nullptr)
    return;

  while (this->protocol_->pop_tx_frame(&this->tx_frame_buf_)) {
    this->tx_char_->set_value(ByteBuffer::wrap(this->tx_frame_buf_.data, this->tx_frame_buf_.len));
    this->tx_char_->notify();
  }
}

bool BuddyTransportBLE::is_tx_ready() const {
  return this->service_ != nullptr;
}

void BuddyTransportBLE::start_custom_advertising_() {
  // Advertisement: flags + UUID — 3 + 18 = 21 bytes (within 31-byte limit)
  esp_ble_adv_data_t adv = {};
  adv.set_scan_rsp     = false;
  adv.include_name     = false;
  adv.include_txpower  = false;
  adv.flag             = ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT;
  adv.service_uuid_len = 16;
  adv.p_service_uuid   = this->adv_uuid_;

  esp_err_t err = esp_ble_gap_config_adv_data(&adv);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "gap_config_adv_data (adv) failed: %s", esp_err_to_name(err));
    return;
  }

  // Scan response: name + txpower — ~11 + 3 = ~14 bytes
  esp_ble_adv_data_t rsp = {};
  rsp.set_scan_rsp    = true;
  rsp.include_name    = true;
  rsp.include_txpower = true;

  err = esp_ble_gap_config_adv_data(&rsp);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "gap_config_adv_data (rsp) failed: %s", esp_err_to_name(err));
    return;
  }

  esp_ble_adv_params_t params = {};
  params.adv_int_min       = 0x20;
  params.adv_int_max       = 0x40;
  params.adv_type          = ADV_TYPE_IND;
  params.own_addr_type     = BLE_ADDR_TYPE_PUBLIC;
  params.channel_map       = ADV_CHNL_ALL;
  params.adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY;
  params.peer_addr_type    = BLE_ADDR_TYPE_PUBLIC;

  err = esp_ble_gap_start_advertising(&params);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "gap_start_advertising failed: %s", esp_err_to_name(err));
  }
}

void BuddyTransportBLE::handle_gap_security_event_(esp_gap_ble_cb_event_t event,
                                                    esp_ble_gap_cb_param_t *param) {
  if (param == nullptr)
    return;
  switch (event) {
    case ESP_GAP_BLE_SEC_REQ_EVT:
      ESP_LOGI(TAG, "BLE security request — accepting");
      esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
      esp_ble_set_encryption(param->ble_security.ble_req.bd_addr, ESP_BLE_SEC_ENCRYPT);
      break;
    case ESP_GAP_BLE_PASSKEY_NOTIF_EVT: {
      uint32_t pk = param->ble_security.key_notif.passkey;
      ESP_LOGI(TAG, "BLE pairing passkey: %06lu", (unsigned long) pk);
      if (this->passkey_callback_) {
        this->passkey_callback_(pk);
      }
      break;
    }
    case ESP_GAP_BLE_AUTH_CMPL_EVT:
      if (param->ble_security.auth_cmpl.success) {
        ESP_LOGI(TAG, "BLE auth complete — link encrypted and bonded");
        this->encrypted_ = true;
      } else {
        ESP_LOGW(TAG, "BLE auth failed: reason=0x%02x", param->ble_security.auth_cmpl.fail_reason);
        this->encrypted_ = false;
      }
      break;
    default:
      break;
  }
}

}  // namespace ai_desktop_buddy
}  // namespace esphome
