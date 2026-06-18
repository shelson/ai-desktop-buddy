#pragma once

#include <cstdint>
#include <array>
#include <lvgl.h>
#include <memory>
#include "esphome/core/automation.h"

namespace esphome {
namespace ai_desktop_buddy {

class BuddyUI {
 public:
  explicit BuddyUI(lv_obj_t *container);
  ~BuddyUI() = default;

  //Setters to receive the actions from the parent
  
  void SetApproveAction(std::unique_ptr<Action<>> action) { approve_action_ = std::move(action); }
  void SetDenyAction(std::unique_ptr<Action<>> action) { deny_action_ = std::move(action); }

  // Static helper functions for LVGL
  static void on_approve_click_static(lv_event_t *event);
  static void on_deny_click_static(lv_event_t *event);

  void set_liveness(bool live);
  // Callers must keep text alive for 2 seconds after the call.
  void push_status_message(const char *text);
  void push_tool_message(const char *tool_name, const char *hint);
  void show_permission(const char *tool, const char *hint);
  void hide_permission();
  void process_loop(uint32_t now_ms);

  bool is_permission_active() const { return this->permission_active_; }
  void nudge_ring(int32_t delta);

private:
  // The actual instance member functions
  void OnApproveButtonClicked(void);
  void OnDenyButtonClicked(void);
  std::unique_ptr<Action<>> approve_action_;
  std::unique_ptr<Action<>> deny_action_;

protected:
  enum class State { DISCONNECTED, IDLE, PERMISSION };

  void transition_to_(State new_state);
  void arrange_arclabel_(lv_obj_t *alabel, int16_t angle, const char *text, bool is_tool);
  lv_obj_t *create_arclabel_();
  void create_permission_card_();
  void create_center_zone_();

  lv_obj_t *container_;
  State state_{State::DISCONNECTED};
  bool permission_active_{false};

  static constexpr size_t kPoolSize = 10;
  std::array<lv_obj_t *, kPoolSize> arclabel_pool_{};
  size_t pool_write_{0};

  lv_group_t *encoder_group_{nullptr};

  lv_obj_t *perm_card_{nullptr};
  lv_obj_t *perm_title_label_{nullptr};
  lv_obj_t *perm_tool_label_{nullptr};
  lv_obj_t *perm_hint_label_{nullptr};
  lv_obj_t *perm_approve_btn_{nullptr};
  lv_obj_t *perm_deny_btn_{nullptr};

  lv_obj_t *center_pulse_{nullptr};
  lv_obj_t *center_live_label_{nullptr};
  lv_obj_t *center_token_label_{nullptr};
  lv_obj_t *center_no_signal_{nullptr};

  lv_style_t style_pulse_{};
  lv_style_t style_no_signal_{};
  lv_style_t style_perm_card_bg_{};
  lv_style_t style_perm_card_border_{};
  lv_style_t style_perm_title_{};
  lv_style_t style_perm_tool_{};
  lv_style_t style_perm_hint_{};
  lv_style_t style_perm_btn_approve_{};
  lv_style_t style_perm_btn_deny_{};
  lv_style_t style_perm_btn_focus_approve_{};
  lv_style_t style_perm_btn_focus_deny_{};
  lv_style_t style_arclabel_cyan_{};
  lv_style_t style_arclabel_magenta_{};

  static constexpr uint32_t kMessageLifetime = 1500;
  static constexpr uint32_t kFadeInDuration = 300;
  static constexpr uint32_t kFadeOutDuration = 500;
  static constexpr int32_t kMessageRadius = 190;
};

}  // namespace ai_desktop_buddy
}  // namespace esphome
