#pragma once

#include <memory>

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "buddy_protocol.h"
#include "buddy_transport_ble.h"
#include "buddy_command_extensions.h"

#ifdef USE_LVGL
#include <lvgl.h>
#endif

namespace esphome {
namespace ai_desktop_buddy {

class BuddyUI;

class AIDesktopBuddy : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return esphome::setup_priority::AFTER_BLUETOOTH + 20; }

  void set_device_name(const std::string &name) { this->transport_.set_device_name(name); }
  void set_liveness_timeout_ms(uint32_t ms) { this->protocol_.set_liveness_timeout_ms(ms); }

  void set_total_sensor(sensor::Sensor *s) { this->total_sensor_ = s; }
  void set_running_sensor(sensor::Sensor *s) { this->running_sensor_ = s; }
  void set_waiting_sensor(sensor::Sensor *s) { this->waiting_sensor_ = s; }
  void set_tokens_sensor(sensor::Sensor *s) { this->tokens_sensor_ = s; }
  void set_tokens_today_sensor(sensor::Sensor *s) { this->tokens_today_sensor_ = s; }
  void set_status_text_sensor(text_sensor::TextSensor *s) { this->status_text_sensor_ = s; }
  void set_prompt_text_sensor(text_sensor::TextSensor *s) { this->prompt_text_sensor_ = s; }
  void set_entries_text_sensor(text_sensor::TextSensor *s) { this->entries_text_sensor_ = s; }
  void set_passkey_text_sensor(text_sensor::TextSensor *s) { this->passkey_text_sensor_ = s; }
  void set_liveness_binary_sensor(binary_sensor::BinarySensor *s) { this->liveness_binary_sensor_ = s; }

  void set_lvgl_ui_container(lv_obj_t *container);

  template<typename F> void add_on_turn_callback(F &&cb) { this->on_turn_cb_.add(std::forward<F>(cb)); }
  template<typename F> void add_on_permission_callback(F &&cb) { this->on_permission_cb_.add(std::forward<F>(cb)); }
  template<typename F> void add_on_liveness_callback(F &&cb) { this->on_liveness_cb_.add(std::forward<F>(cb)); }
  template<typename F> void add_on_command_callback(F &&cb) { this->on_command_cb_.add(std::forward<F>(cb)); }

  void action_approve();
  void action_deny();

 protected:
  void handle_protocol_event_(const BuddyEvent &event);
  void publish_sensors_();

  std::string build_entries_json_();
  std::string build_prompt_json_();
  std::string build_status_json_();

  BuddyProtocol protocol_;
  BuddyTransportBLE transport_;
  BuddyCommandExtensions command_extensions_;

  sensor::Sensor *total_sensor_{nullptr};
  sensor::Sensor *running_sensor_{nullptr};
  sensor::Sensor *waiting_sensor_{nullptr};
  sensor::Sensor *tokens_sensor_{nullptr};
  sensor::Sensor *tokens_today_sensor_{nullptr};
  text_sensor::TextSensor *status_text_sensor_{nullptr};
  text_sensor::TextSensor *prompt_text_sensor_{nullptr};
  text_sensor::TextSensor *entries_text_sensor_{nullptr};
  text_sensor::TextSensor *passkey_text_sensor_{nullptr};
  binary_sensor::BinarySensor *liveness_binary_sensor_{nullptr};

  lv_obj_t *lvgl_ui_container_{nullptr};

  std::unique_ptr<BuddyUI> ui_;

  CallbackManager<void(const std::string &, const std::string &)> on_turn_cb_{};
  CallbackManager<void(const std::string &, const std::string &, const std::string &)> on_permission_cb_{};
  CallbackManager<void(bool)> on_liveness_cb_{};
  CallbackManager<void(const std::string &, const std::string &)> on_command_cb_{};

  std::string device_name_;
  uint32_t passkey_{0};
};

template<typename... Ts> class AIDesktopBuddyApproveAction : public Action<Ts...> {
 public:
  explicit AIDesktopBuddyApproveAction(AIDesktopBuddy *parent) : parent_(parent) {}
  void play(const Ts &...x) override { this->parent_->action_approve(); }
 protected:
  AIDesktopBuddy *parent_;
};

template<typename... Ts> class AIDesktopBuddyDenyAction : public Action<Ts...> {
 public:
  explicit AIDesktopBuddyDenyAction(AIDesktopBuddy *parent) : parent_(parent) {}
  void play(const Ts &...x) override { this->parent_->action_deny(); }
 protected:
  AIDesktopBuddy *parent_;
};

}  // namespace ai_desktop_buddy
}  // namespace esphome
