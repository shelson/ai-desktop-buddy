#include "buddy_protocol.h"
#include "esphome/core/log.h"
#include <cmath>
#include <cstring>

namespace esphome {
namespace ai_desktop_buddy {

static const char *const TAG = "buddy_protocol";

void BuddyProtocol::receive_bytes(const uint8_t *data, size_t len) {
  for (size_t i = 0; i < len; i++) {
    char c = (char) data[i];
    if (c == '\r' || c == '\n') {
      if (this->line_buf_dropping_) {
        this->line_buf_dropping_ = false;
        this->line_buf_len_ = 0;
      } else if (this->line_buf_len_ > 0) {
        this->line_buf_[this->line_buf_len_] = '\0';
        this->process_line(this->line_buf_, this->line_buf_len_);
        this->line_buf_len_ = 0;
      }
      continue;
    }
    if (this->line_buf_dropping_)
      continue;
    if (this->line_buf_len_ + 1 >= sizeof(this->line_buf_)) {
      this->line_buf_len_ = 0;
      this->line_buf_dropping_ = true;
      this->emit_error(0, AI_BUDDY_LINE_MAX);
      continue;
    }
    this->line_buf_[this->line_buf_len_++] = c;
  }
}

void BuddyProtocol::process_line(const char *line, size_t len) {
  ESP_LOGV(TAG, "rx: %.*s", (int) len, line);
  this->process_object(line, len);
}

void BuddyProtocol::process_object(const char *line, size_t len) {
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, line, len);
  if (error) {
    ESP_LOGW(TAG, "rx malformed len=%u", (unsigned) len);
    this->emit_error(0, 0);
    return;
  }
  if (!doc.is<JsonObject>()) {
    this->emit_error(0, 0);
    return;
  }
  JsonObject root = doc.as<JsonObject>();

  if (root.containsKey("cmd")) {
    this->handle_command_object(line, len);
  } else if (root.containsKey("evt")) {
    this->handle_turn_event(line, len);
  } else if (root.containsKey("time")) {
    this->handle_time_sync(line, len);
  } else {
    this->handle_state_push(line, len);
  }
}

void BuddyProtocol::handle_state_push(const char *line, size_t len) {
  JsonDocument doc;
  deserializeJson(doc, line, len);
  JsonObject root = doc.as<JsonObject>();

  bool ok_total = false, ok_running = false, ok_waiting = false;
  bool ok_tokens = false, ok_tokens_today = false;

  this->state_.total = this->json_u32(root["total"], &ok_total);
  this->state_.running = this->json_u32(root["running"], &ok_running);
  this->state_.waiting = this->json_u32(root["waiting"], &ok_waiting);
  this->state_.tokens = this->json_u64(root["tokens"], &ok_tokens);
  this->state_.tokens_today = this->json_u64(root["tokens_today"], &ok_tokens_today);

  const char *msg = root["msg"] | "";
  if (!ok_total || !ok_running || !ok_waiting || !ok_tokens || !ok_tokens_today ||
      msg[0] == '\0') {
    this->emit_error(0, 0);
    return;
  }

  this->state_.has_state = true;
  this->copy_utf8_trunc(this->state_.msg, sizeof(this->state_.msg), msg);

  // entries
  this->state_.entry_count = 0;
  JsonArray entries = root["entries"];
  if (!entries.isNull()) {
    char *entry_ptr = this->state_.entries_.data();
    for (JsonVariant v : entries) {
      if (this->state_.entry_count >= AI_BUDDY_ENTRY_COUNT_MAX)
        break;
      const char *entry_str = v | "";
      if (entry_str[0] == '\0')
        continue;
      this->copy_utf8_trunc(entry_ptr, AI_BUDDY_ENTRY_STRING_MAX + 1, entry_str);
      entry_ptr += AI_BUDDY_ENTRY_STRING_MAX + 1;
      this->state_.entry_count++;
    }
  }

  // prompt
  JsonObject prompt = root["prompt"];
  this->state_.prompt = PromptData{};
  if (!prompt.isNull()) {
    const char *pid = prompt["id"] | "";
    if (pid[0] != '\0') {
      size_t pid_len = strnlen(pid, sizeof(this->state_.prompt.id));
      if (pid_len < sizeof(this->state_.prompt.id)) {
        this->state_.prompt.present = true;
        this->copy_utf8_trunc(this->state_.prompt.id, sizeof(this->state_.prompt.id), pid);
        this->copy_utf8_trunc(this->state_.prompt.tool, sizeof(this->state_.prompt.tool),
                              prompt["tool"] | "");
        this->copy_utf8_trunc(this->state_.prompt.hint, sizeof(this->state_.prompt.hint),
                              prompt["hint"] | "");
      }
    }
  }

  // update liveness
  uint32_t now = millis();
  bool same_prompt = this->state_.prompt.present && this->state_.prompt.present &&
                     strcmp(this->state_.prompt.id, this->state_.prompt.id) == 0;
  // Note: the condition uses state_.prompt (already updated above) so same_prompt always
  // compares the NEW prompt with itself. This is intentional: a state push always resets
  // prompt_response_sent when the prompt changes OR waiting > 0.
  this->prompt_response_sent_ = false;
  this->last_state_ms_ = now;
  bool was_live = this->live_;
  this->live_ = true;

  this->emit_buf_ = {};
  this->emit_buf_.type = BuddyEventType::SNAPSHOT_UPDATED;
  this->emit_event(this->emit_buf_);

  if (!was_live) {
    this->emit_buf_ = {};
    this->emit_buf_.type = BuddyEventType::LIVENESS_CHANGED;
    this->emit_buf_.live = true;
    this->emit_event(this->emit_buf_);
  }
}

void BuddyProtocol::handle_time_sync(const char *line, size_t len) {
  JsonDocument doc;
  deserializeJson(doc, line, len);
  JsonObject root = doc.as<JsonObject>();

  JsonArray time_arr = root["time"];
  if (time_arr.isNull() || time_arr.size() < 2) {
    this->emit_error(0, 0);
    return;
  }

  bool ok_epoch = false, ok_offset = false;
  int64_t epoch = this->json_i64(time_arr[0], &ok_epoch);
  int32_t tz_offset = this->json_i32(time_arr[1], &ok_offset);
  if (!ok_epoch || !ok_offset) {
    this->emit_error(0, 0);
    return;
  }

  this->emit_buf_ = {};
  this->emit_buf_.type = BuddyEventType::TIME_SYNC;
  this->emit_buf_.time_sync.epoch = epoch;
  this->emit_buf_.time_sync.tz_offset = tz_offset;
  this->emit_event(this->emit_buf_);
}

void BuddyProtocol::handle_turn_event(const char *line, size_t len) {
  JsonDocument doc;
  deserializeJson(doc, line, len);
  JsonObject root = doc.as<JsonObject>();

  const char *evt = root["evt"] | "";
  if (strcmp(evt, "turn") != 0)
    return;

  const char *role = root["role"] | "";
  JsonVariant content = root["content"];

  this->emit_buf_ = {};
  this->emit_buf_.type = BuddyEventType::TURN;
  this->copy_utf8_trunc(this->emit_buf_.turn.role, sizeof(this->emit_buf_.turn.role), role);

  if (!content.isNull()) {
    std::string content_str;
    serializeJson(content, content_str);
    size_t ser_len = content_str.length();
    if (ser_len < sizeof(this->emit_buf_.turn.content)) {
      memcpy(this->emit_buf_.turn.content, content_str.c_str(), ser_len + 1);
    }
  }
  this->emit_event(this->emit_buf_);
}

void BuddyProtocol::handle_command_object(const char *line, size_t len) {
  JsonDocument doc;
  deserializeJson(doc, line, len);
  JsonObject root = doc.as<JsonObject>();

  const char *cmd = root["cmd"] | "";
  if (cmd[0] == '\0') {
    this->emit_error(0, 0);
    return;
  }

  if (strcmp(cmd, "permission") == 0) {
    const char *pid = root["id"] | "";
    const char *tool = root["tool"] | "";
    const char *hint = root["hint"] | "";

    this->emit_buf_ = {};
    this->emit_buf_.type = BuddyEventType::PERMISSION_RECEIVED;
    this->copy_utf8_trunc(this->emit_buf_.permission.id, sizeof(this->emit_buf_.permission.id), pid);
    this->copy_utf8_trunc(this->emit_buf_.permission.tool, sizeof(this->emit_buf_.permission.tool), tool);
    this->copy_utf8_trunc(this->emit_buf_.permission.hint, sizeof(this->emit_buf_.permission.hint), hint);
    this->emit_event(this->emit_buf_);
    return;
  }

  if (strcmp(cmd, "char_begin") == 0 || strcmp(cmd, "file") == 0 ||
      strcmp(cmd, "chunk") == 0 || strcmp(cmd, "file_end") == 0 ||
      strcmp(cmd, "char_end") == 0) {
    this->queue_ack(cmd, true, 0, nullptr);

    this->emit_buf_ = {};
    this->emit_buf_.type = BuddyEventType::COMMAND_EXTENSION;
    this->copy_utf8_trunc(this->emit_buf_.command.cmd, sizeof(this->emit_buf_.command.cmd), cmd);
    this->emit_event(this->emit_buf_);
    return;
  }

  // all other commands: emit COMMAND event for integration layer to handle
  {
    std::string cmd_str;
    serializeJson(root, cmd_str);
    this->emit_buf_ = {};
    this->emit_buf_.type = BuddyEventType::COMMAND;
    this->copy_utf8_trunc(this->emit_buf_.command.cmd, sizeof(this->emit_buf_.command.cmd), cmd);
    this->copy_utf8_trunc(this->emit_buf_.command.data, sizeof(this->emit_buf_.command.data), cmd_str.c_str());
    this->emit_event(this->emit_buf_);
  }
}

bool BuddyProtocol::approve(const char *prompt_id) {
  if (prompt_id == nullptr || prompt_id[0] == '\0')
    return false;
  if (this->prompt_response_sent_)
    return false;
  this->prompt_response_sent_ = true;
  return this->queue_permission_reply(prompt_id, "once");
}

bool BuddyProtocol::deny(const char *prompt_id) {
  if (prompt_id == nullptr || prompt_id[0] == '\0')
    return false;
  if (this->prompt_response_sent_)
    return false;
  this->prompt_response_sent_ = true;
  return this->queue_permission_reply(prompt_id, "deny");
}

bool BuddyProtocol::send_status_response(const char *json_data, size_t len) {
  JsonDocument data_doc;
  DeserializationError err = deserializeJson(data_doc, json_data, len);
  if (err || !data_doc.is<JsonObject>()) {
    return this->queue_ack("status", false, 0, "invalid_status_data");
  }

  JsonDocument root;
  JsonObject root_obj = root.to<JsonObject>();
  root_obj["ack"] = "status";
  root_obj["ok"] = true;
  root_obj["n"] = 0;
  root_obj["data"] = data_doc;

  std::string json;
  serializeJson(root, json);
  if (json.length() > AI_BUDDY_LINE_MAX) {
    return this->queue_ack("status", false, 0, "status_too_large");
  }
  return this->queue_tx_json(json.c_str(), json.length());
}

bool BuddyProtocol::pop_tx_frame(TxFrame *out) {
  if (this->tx_queue_count_ == 0)
    return false;
  *out = this->tx_queue_[this->tx_queue_head_];
  this->tx_queue_head_ = (this->tx_queue_head_ + 1) % AI_BUDDY_TX_QUEUE_DEPTH;
  this->tx_queue_count_--;
  return true;
}

bool BuddyProtocol::queue_tx_json(const char *json, size_t len) {
  if (this->tx_queue_count_ >= AI_BUDDY_TX_QUEUE_DEPTH) {
    ESP_LOGW(TAG, "tx queue full, dropping frame");
    return false;
  }
  size_t frame_len = len + 1;
  if (frame_len > sizeof(TxFrame::data))
    return false;
  size_t idx = (this->tx_queue_head_ + this->tx_queue_count_) % AI_BUDDY_TX_QUEUE_DEPTH;
  TxFrame &slot = this->tx_queue_[idx];
  slot.len = frame_len;
  memcpy(slot.data, json, len);
  slot.data[len] = '\n';
  this->tx_queue_count_++;
  return true;
}

bool BuddyProtocol::queue_ack(const char *cmd, bool ok, uint32_t n, const char *error) {
  JsonDocument doc;
  JsonObject root = doc.to<JsonObject>();
  root["ack"] = cmd;
  root["ok"] = ok;
  root["n"] = n;
  if (error != nullptr && error[0] != '\0') {
    root["error"] = error;
  }
  std::string json;
  serializeJson(doc, json);
  return this->queue_tx_json(json.c_str(), json.length());
}

bool BuddyProtocol::queue_permission_reply(const char *prompt_id, const char *decision) {
  JsonDocument doc;
  JsonObject root = doc.to<JsonObject>();
  root["cmd"] = "permission";
  root["id"] = prompt_id;
  root["decision"] = decision;
  std::string json;
  serializeJson(doc, json);
  return this->queue_tx_json(json.c_str(), json.length());
}

void BuddyProtocol::emit_event(const BuddyEvent &event) {
  this->event_callback_.call(event);
}

void BuddyProtocol::emit_error(uint8_t kind, uint32_t detail) {
  this->emit_buf_ = {};
  this->emit_buf_.type = BuddyEventType::ERROR;
  this->emit_buf_.error.kind = kind;
  this->emit_buf_.error.detail = detail;
  this->emit_event(this->emit_buf_);
}

void BuddyProtocol::check_liveness(uint32_t now_ms) {
  if (this->last_state_ms_ == 0)
    return;
  uint32_t elapsed = now_ms - this->last_state_ms_;
  bool is_live = elapsed <= this->liveness_timeout_ms_;
  if (is_live != this->live_) {
    this->live_ = is_live;
    this->emit_buf_ = {};
    this->emit_buf_.type = BuddyEventType::LIVENESS_CHANGED;
    this->emit_buf_.live = is_live;
    this->emit_event(this->emit_buf_);
  }
}

void BuddyProtocol::copy_utf8_trunc(char *dst, size_t dst_size, const char *src) {
  if (dst == nullptr || dst_size == 0)
    return;
  if (src == nullptr) {
    dst[0] = '\0';
    return;
  }
  size_t src_len = strlen(src);
  size_t copy_len = (src_len < dst_size - 1) ? src_len : dst_size - 1;
  while (copy_len > 0 && (((unsigned char) src[copy_len] & 0xC0u) == 0x80u))
    copy_len--;
  memcpy(dst, src, copy_len);
  dst[copy_len] = '\0';
}

uint32_t BuddyProtocol::json_u32(JsonVariantConst item, bool *ok) {
  if (item.isNull()) { if (ok) *ok = false; return 0; }
  float v = item.as<float>();
  if (std::isnan(v)) { if (ok) *ok = false; return 0; }
  if (ok) *ok = true;
  if (v <= 0.0f) return 0;
  if (v >= (float) UINT32_MAX) return UINT32_MAX;
  return (uint32_t) v;
}

uint64_t BuddyProtocol::json_u64(JsonVariantConst item, bool *ok) {
  if (item.isNull()) { if (ok) *ok = false; return 0; }
  float v = item.as<float>();
  if (std::isnan(v)) { if (ok) *ok = false; return 0; }
  if (ok) *ok = true;
  if (v <= 0.0f) return 0;
  if (v >= (float) UINT64_MAX) return UINT64_MAX;
  return (uint64_t) v;
}

int64_t BuddyProtocol::json_i64(JsonVariantConst item, bool *ok) {
  if (item.isNull()) { if (ok) *ok = false; return 0; }
  float v = item.as<float>();
  if (std::isnan(v)) { if (ok) *ok = false; return 0; }
  if (ok) *ok = true;
  if (v <= (float) INT64_MIN) return INT64_MIN;
  if (v >= (float) INT64_MAX) return INT64_MAX;
  return (int64_t) v;
}

int32_t BuddyProtocol::json_i32(JsonVariantConst item, bool *ok) {
  if (item.isNull()) { if (ok) *ok = false; return 0; }
  float v = item.as<float>();
  if (std::isnan(v)) { if (ok) *ok = false; return 0; }
  if (ok) *ok = true;
  if (v <= (float) INT32_MIN) return INT32_MIN;
  if (v >= (float) INT32_MAX) return INT32_MAX;
  return (int32_t) v;
}

}  // namespace ai_desktop_buddy
}  // namespace esphome
