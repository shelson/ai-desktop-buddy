#pragma once

#include <array>
#include <cstdint>
#include <cstddef>
#include <functional>
#define ARDUINOJSON_ENABLE_STD_STRING 1
#define ARDUINOJSON_USE_LONG_LONG 1
#define ARDUINOJSON_USE_DOUBLE 1
#include <ArduinoJson.h>
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "buddy_state.h"

namespace esphome {
namespace ai_desktop_buddy {

class BuddyProtocol {
 public:
  void receive_bytes(const uint8_t *data, size_t len);

  void set_liveness_timeout_ms(uint32_t ms) { this->liveness_timeout_ms_ = ms; }
  uint32_t get_liveness_timeout_ms() const { return this->liveness_timeout_ms_; }

  bool get_live() const { return this->live_; }
  const BuddyState &get_state() const { return this->state_; }

  template<typename F> void add_event_callback(F &&callback) {
    this->event_callback_.add(std::forward<F>(callback));
  }

  bool approve(const char *prompt_id);
  bool deny(const char *prompt_id);

  bool pop_tx_frame(TxFrame *out);
  bool send_status_response(const char *json_data, size_t len);
  bool queue_ack(const char *cmd, bool ok, uint32_t n, const char *error);

  void check_liveness(uint32_t now_ms);

 protected:
  void process_line(const char *line, size_t len);
  void process_object(const char *line, size_t len);
  void handle_state_push(const char *line, size_t len);
  void handle_time_sync(const char *line, size_t len);
  void handle_turn_event(const char *line, size_t len);
  void handle_command_object(const char *line, size_t len);

  void emit_event(const BuddyEvent &event);
  void emit_error(uint8_t kind, uint32_t detail);

  bool queue_tx_json(const char *json, size_t len);
  bool queue_permission_reply(const char *prompt_id, const char *decision);

  void copy_utf8_trunc(char *dst, size_t dst_size, const char *src);
  uint32_t json_u32(JsonVariantConst item, bool *ok);
  uint64_t json_u64(JsonVariantConst item, bool *ok);
  int64_t json_i64(JsonVariantConst item, bool *ok);
  int32_t json_i32(JsonVariantConst item, bool *ok);

  uint32_t liveness_timeout_ms_{30000};

  char line_buf_[AI_BUDDY_LINE_MAX + 1]{};
  size_t line_buf_len_{0};
  bool line_buf_dropping_{false};

  BuddyState state_{};
  uint32_t last_state_ms_{0};
  bool live_{false};
  bool prompt_response_sent_{false};

  std::array<TxFrame, AI_BUDDY_TX_QUEUE_DEPTH> tx_queue_{};
  size_t tx_queue_head_{0};
  size_t tx_queue_count_{0};
  BuddyEvent emit_buf_{};
  CallbackManager<void(const BuddyEvent &)> event_callback_{};
};

}  // namespace ai_desktop_buddy
}  // namespace esphome
