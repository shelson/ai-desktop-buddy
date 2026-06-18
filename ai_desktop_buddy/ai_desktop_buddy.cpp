#include "ai_desktop_buddy.h"
#include "buddy_ui.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ai_desktop_buddy {

static const char *const TAG = "ai_desktop_buddy";

void AIDesktopBuddy::setup() {
  this->protocol_.add_event_callback([this](const BuddyEvent &event) {
    this->handle_protocol_event_(event);
  });

  this->transport_.set_passkey_callback([this](uint32_t pk) {
    this->passkey_ = pk;
    if (this->passkey_text_sensor_ != nullptr) {
      char buf[8];
      snprintf(buf, sizeof(buf), "%06lu", (unsigned long) pk);
      this->passkey_text_sensor_->publish_state(buf);
    }
    ESP_LOGI(TAG, "Pairing passkey: %06lu", (unsigned long) pk);
  });

  this->transport_.setup(&this->protocol_);

  if (this->lvgl_ui_container_ != nullptr) {
    this->ui_ = std::make_unique<BuddyUI>(this->lvgl_ui_container_);
  }
  // 2. Instantiate the template actions (assuming zero arguments <>)
    auto approveAction = std::make_unique<AIDesktopBuddyApproveAction<>>(this);
    auto denyAction = std::make_unique<AIDesktopBuddyDenyAction<>>(this); // Assuming you have a Deny class too

    // 3. Pass ownership of these actions to the UI
    ui_->SetApproveAction(std::move(approveAction));
    ui_->SetDenyAction(std::move(denyAction));

  this->device_name_ = this->transport_.get_device_name();
  ESP_LOGI(TAG, "Setup complete. Device name: %s", this->device_name_.c_str());
}

void AIDesktopBuddy::loop() {
  this->transport_.loop();

  uint32_t now = millis();
  this->protocol_.check_liveness(now);
  if (this->ui_) {
    this->ui_->process_loop(now);
  }
}

void AIDesktopBuddy::dump_config() {
  ESP_LOGCONFIG(TAG, "AI Desktop Buddy:");
  ESP_LOGCONFIG(TAG, "  Device Name: %s", this->device_name_.c_str());
  ESP_LOGCONFIG(TAG, "  Liveness Timeout: %u ms", this->protocol_.get_liveness_timeout_ms());
  if (this->lvgl_ui_container_ != nullptr)
    ESP_LOGCONFIG(TAG, "  LVGL UI Container: bound");
}

void AIDesktopBuddy::handle_protocol_event_(const BuddyEvent &event) {
  switch (event.type) {
    case BuddyEventType::SNAPSHOT_UPDATED:
      this->publish_sensors_();
      {
        const auto &state = this->protocol_.get_state();
        if (this->ui_ != nullptr) {
          if (!this->ui_->is_permission_active() && state.msg[0] != '\0') {
            this->ui_->push_status_message(state.msg);
          }
          if (state.prompt.present && !this->ui_->is_permission_active()) {
            this->ui_->show_permission(state.prompt.tool, state.prompt.hint);
          }
        }
      }
      break;

    case BuddyEventType::PERMISSION_RECEIVED:
      this->on_permission_cb_.call(
          std::string(event.permission.id),
          std::string(event.permission.tool),
          std::string(event.permission.hint));
      if (this->ui_ != nullptr) {
        this->ui_->show_permission(event.permission.tool, event.permission.hint);
      }
      break;

    case BuddyEventType::TURN:
      this->on_turn_cb_.call(
          std::string(event.turn.role),
          std::string(event.turn.content));
      break;

    case BuddyEventType::LIVENESS_CHANGED:
      this->on_liveness_cb_.call(event.live);
      if (this->liveness_binary_sensor_ != nullptr)
        this->liveness_binary_sensor_->publish_state(event.live);
      if (this->ui_ != nullptr) {
        this->ui_->set_liveness(event.live);
      }
      break;

    case BuddyEventType::COMMAND: {
      this->on_command_cb_.call(
          std::string(event.command.cmd),
          std::string(event.command.data));

      if (strcmp(event.command.cmd, "status") == 0) {
        std::string status_json = this->build_status_json_();
        this->protocol_.send_status_response(status_json.c_str(), status_json.length());
      } else if (strcmp(event.command.cmd, "name") == 0) {
        this->protocol_.queue_ack("name", true, 0, nullptr);
      } else if (strcmp(event.command.cmd, "owner") == 0) {
        this->protocol_.queue_ack("owner", true, 0, nullptr);
      } else if (strcmp(event.command.cmd, "unpair") == 0) {
        this->protocol_.queue_ack("unpair", true, 0, nullptr);
      }
      break;
    }

    case BuddyEventType::TIME_SYNC:
      ESP_LOGD(TAG, "Time sync: epoch=%lld tz_offset=%ld",
               event.time_sync.epoch, event.time_sync.tz_offset);
      break;

    case BuddyEventType::ERROR:
      ESP_LOGW(TAG, "Protocol error: kind=%u detail=%u",
               event.error.kind, event.error.detail);
      break;

    case BuddyEventType::COMMAND_EXTENSION:
      break;
  }
}

void AIDesktopBuddy::publish_sensors_() {
  const auto &state = this->protocol_.get_state();
  if (!state.has_state)
    return;

  if (this->total_sensor_)
    this->total_sensor_->publish_state(state.total);
  if (this->running_sensor_)
    this->running_sensor_->publish_state(state.running);
  if (this->waiting_sensor_)
    this->waiting_sensor_->publish_state(state.waiting);
  if (this->tokens_sensor_)
    this->tokens_sensor_->publish_state(state.tokens);
  if (this->tokens_today_sensor_)
    this->tokens_today_sensor_->publish_state(state.tokens_today);
  if (this->status_text_sensor_)
    this->status_text_sensor_->publish_state(state.msg);
  if (this->prompt_text_sensor_) {
    std::string json = this->build_prompt_json_();
    this->prompt_text_sensor_->publish_state(json);
  }
  if (this->entries_text_sensor_) {
    std::string json = this->build_entries_json_();
    this->entries_text_sensor_->publish_state(json);
  }
}

std::string AIDesktopBuddy::build_entries_json_() {
  const auto &state = this->protocol_.get_state();
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  const char *entry_ptr = state.entries_.data();
  for (size_t i = 0; i < state.entry_count; i++) {
    arr.add(entry_ptr);
    entry_ptr += AI_BUDDY_ENTRY_STRING_MAX + 1;
  }
  std::string out;
  serializeJson(doc, out);
  return out;
}

std::string AIDesktopBuddy::build_prompt_json_() {
  const auto &prompt = this->protocol_.get_state().prompt;
  if (!prompt.present)
    return "{}";
  JsonDocument doc;
  JsonObject obj = doc.to<JsonObject>();
  obj["id"] = prompt.id;
  obj["tool"] = prompt.tool;
  obj["hint"] = prompt.hint;
  std::string out;
  serializeJson(doc, out);
  return out;
}

std::string AIDesktopBuddy::build_status_json_() {
  JsonDocument doc;
  JsonObject obj = doc.to<JsonObject>();
  obj["name"] = this->device_name_;
  obj["owner"] = "";
  obj["sec"] = true;
  JsonObject sys = obj["sys"].to<JsonObject>();
  sys["up"] = millis() / 1000;
  sys["heap"] = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  JsonObject stats = obj["stats"].to<JsonObject>();
  stats["appr"] = 0;
  stats["deny"] = 0;
  std::string out;
  serializeJson(doc, out);
  return out;
}

void AIDesktopBuddy::set_lvgl_ui_container(lv_obj_t *container) {
  this->lvgl_ui_container_ = container;
}

void AIDesktopBuddy::action_approve() {
  const auto &prompt = this->protocol_.get_state().prompt;
  if (prompt.present) {
    this->protocol_.approve(prompt.id);
    if (this->ui_ != nullptr) {
      this->ui_->hide_permission();
    }
  }
}

void AIDesktopBuddy::action_deny() {
  const auto &prompt = this->protocol_.get_state().prompt;
  if (prompt.present) {
    this->protocol_.deny(prompt.id);
    if (this->ui_ != nullptr) {
      this->ui_->hide_permission();
    }
  }
}

}  // namespace ai_desktop_buddy
}  // namespace esphome
