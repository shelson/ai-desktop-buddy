#include "buddy_ui.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include <cstdlib>
#include <cmath>
#include <cstring>

namespace esphome {
namespace ai_desktop_buddy {

static const char *const TAG = "buddy_ui";

BuddyUI::BuddyUI(lv_obj_t *container) : container_(container) {
  if (this->container_ == nullptr)
    return;

    // Get the encoder group so we can assign it to the approve/deny buttons
    this->encoder_group_ = lv_obj_get_group(this->container_);
    if (this->encoder_group_ == nullptr) {
      ESP_LOGW(TAG, "Can't get encoder group handle, buttons won't work!");
      return;
    }


  for (size_t i = 0; i < kPoolSize; i++) {
    this->arclabel_pool_[i] = this->create_arclabel_();
  }

  lv_style_init(&this->style_pulse_);
  lv_style_set_bg_color(&this->style_pulse_, lv_color_hex(0x00F0FF));
  lv_style_set_bg_opa(&this->style_pulse_, LV_OPA_COVER);
  lv_style_set_radius(&this->style_pulse_, LV_RADIUS_CIRCLE);
  lv_style_set_shadow_color(&this->style_pulse_, lv_color_hex(0x00F0FF));
  lv_style_set_shadow_width(&this->style_pulse_, 12);
  lv_style_set_shadow_opa(&this->style_pulse_, LV_OPA_60);

  lv_style_init(&this->style_no_signal_);
  lv_style_set_text_color(&this->style_no_signal_, lv_color_hex(0xFF2244));
  lv_style_set_text_opa(&this->style_no_signal_, LV_OPA_60);

  lv_style_init(&this->style_arclabel_cyan_);
  lv_style_set_text_color(&this->style_arclabel_cyan_, lv_color_hex(0x00F0FF));
  lv_style_set_opa(&this->style_arclabel_cyan_, LV_OPA_COVER);

  lv_style_init(&this->style_arclabel_magenta_);
  lv_style_set_text_color(&this->style_arclabel_magenta_, lv_color_hex(0xFF00AA));
  lv_style_set_opa(&this->style_arclabel_magenta_, LV_OPA_COVER);

  lv_style_init(&this->style_perm_card_bg_);
  lv_style_set_bg_color(&this->style_perm_card_bg_, lv_color_hex(0x000F19));
  lv_style_set_bg_opa(&this->style_perm_card_bg_, LV_OPA_70);
  lv_style_set_radius(&this->style_perm_card_bg_, 8);

  lv_style_init(&this->style_perm_card_border_);
  lv_style_set_border_color(&this->style_perm_card_border_, lv_color_hex(0xFF00AA));
  lv_style_set_border_opa(&this->style_perm_card_border_, LV_OPA_50);
  lv_style_set_border_width(&this->style_perm_card_border_, 1);

  lv_style_init(&this->style_perm_title_);
  lv_style_set_text_color(&this->style_perm_title_, lv_color_hex(0xFF00AA));
  lv_style_set_text_opa(&this->style_perm_title_, LV_OPA_COVER);

  lv_style_init(&this->style_perm_tool_);
  lv_style_set_text_color(&this->style_perm_tool_, lv_color_hex(0xFFFFFF));
  lv_style_set_text_opa(&this->style_perm_tool_, LV_OPA_COVER);

  lv_style_init(&this->style_perm_hint_);
  lv_style_set_text_color(&this->style_perm_hint_, lv_color_hex(0xFFAA00));
  lv_style_set_text_opa(&this->style_perm_hint_, LV_OPA_COVER);

  lv_style_init(&this->style_perm_btn_approve_);
  lv_style_set_bg_color(&this->style_perm_btn_approve_, lv_color_hex(0x002211));
  lv_style_set_border_color(&this->style_perm_btn_approve_, lv_color_hex(0x00FF66));
  lv_style_set_border_width(&this->style_perm_btn_approve_, 1);
  lv_style_set_text_color(&this->style_perm_btn_approve_, lv_color_hex(0x00FF66));
  lv_style_set_radius(&this->style_perm_btn_approve_, 4);

  lv_style_init(&this->style_perm_btn_deny_);
  lv_style_set_bg_color(&this->style_perm_btn_deny_, lv_color_hex(0x220011));
  lv_style_set_border_color(&this->style_perm_btn_deny_, lv_color_hex(0xFF2244));
  lv_style_set_border_width(&this->style_perm_btn_deny_, 1);
  lv_style_set_text_color(&this->style_perm_btn_deny_, lv_color_hex(0xFF2244));
  lv_style_set_radius(&this->style_perm_btn_deny_, 4);

  lv_style_init(&this->style_perm_btn_focus_approve_);
  lv_style_set_shadow_color(&this->style_perm_btn_focus_approve_, lv_color_hex(0x00FF66));
  lv_style_set_shadow_width(&this->style_perm_btn_focus_approve_, 12);
  lv_style_set_shadow_opa(&this->style_perm_btn_focus_approve_, LV_OPA_80);

  lv_style_init(&this->style_perm_btn_focus_deny_);
  lv_style_set_shadow_color(&this->style_perm_btn_focus_deny_, lv_color_hex(0xFF2244));
  lv_style_set_shadow_width(&this->style_perm_btn_focus_deny_, 12);
  lv_style_set_shadow_opa(&this->style_perm_btn_focus_deny_, LV_OPA_80);

  this->create_center_zone_();
  this->create_permission_card_();
  this->transition_to_(State::DISCONNECTED);

  srand(millis());
}

lv_obj_t *BuddyUI::create_arclabel_() {
  lv_obj_t *label = lv_label_create(this->container_);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_obj_set_style_text_color(label, lv_color_hex(0x00F0FF), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(label, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_opa(label, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_label_set_text(label, "");
  lv_obj_add_flag(label, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_style_opa(label, LV_OPA_TRANSP, LV_PART_MAIN);
  return label;
}

void BuddyUI::create_center_zone_() {
  this->center_pulse_ = lv_obj_create(this->container_);
  lv_obj_set_size(this->center_pulse_, 12, 12);
  lv_obj_add_style(this->center_pulse_, &this->style_pulse_, LV_PART_MAIN);
  lv_obj_center(this->center_pulse_);
  lv_obj_set_y(this->center_pulse_, -16);

  this->center_live_label_ = lv_label_create(this->container_);
  lv_label_set_text(this->center_live_label_, "L I V E");
  lv_obj_set_style_text_color(this->center_live_label_, lv_color_hex(0xC0F0FF), LV_PART_MAIN);
  lv_obj_center(this->center_live_label_);
  lv_obj_set_y(this->center_live_label_, -4);

  this->center_token_label_ = lv_label_create(this->container_);
  lv_label_set_text(this->center_token_label_, "0");
  lv_obj_set_style_text_font(this->center_token_label_, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_obj_set_style_text_color(this->center_token_label_, lv_color_hex(0xC0F0FF), LV_PART_MAIN);
  lv_obj_center(this->center_token_label_);
  lv_obj_set_y(this->center_token_label_, 16);

  this->center_no_signal_ = lv_label_create(this->container_);
  lv_label_set_text(this->center_no_signal_, "NO SIGNAL");
  lv_obj_add_style(this->center_no_signal_, &this->style_no_signal_, LV_PART_MAIN);
  lv_obj_center(this->center_no_signal_);
  lv_obj_add_flag(this->center_no_signal_, LV_OBJ_FLAG_HIDDEN);
}

void BuddyUI::create_permission_card_() {
  this->perm_card_ = lv_obj_create(this->container_);
  lv_obj_set_size(this->perm_card_, 275, 192);
  lv_obj_add_style(this->perm_card_, &this->style_perm_card_bg_, LV_PART_MAIN);
  lv_obj_add_style(this->perm_card_, &this->style_perm_card_border_, LV_PART_MAIN);
  lv_obj_center(this->perm_card_);
  lv_obj_add_flag(this->perm_card_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(this->perm_card_, LV_OBJ_FLAG_SCROLLABLE);

  this->perm_title_label_ = lv_label_create(this->perm_card_);
  lv_label_set_text(this->perm_title_label_, "PERMISSION REQUIRED");
  lv_obj_add_style(this->perm_title_label_, &this->style_perm_title_, LV_PART_MAIN);
  lv_obj_align(this->perm_title_label_, LV_ALIGN_TOP_MID, 0, 12);

  this->perm_tool_label_ = lv_label_create(this->perm_card_);
  lv_label_set_text(this->perm_tool_label_, "");
  lv_obj_add_style(this->perm_tool_label_, &this->style_perm_tool_, LV_PART_MAIN);
  lv_obj_align(this->perm_tool_label_, LV_ALIGN_TOP_MID, 0, 24);

  this->perm_hint_label_ = lv_label_create(this->perm_card_);
  lv_label_set_text(this->perm_hint_label_, "");
  lv_obj_add_style(this->perm_hint_label_, &this->style_perm_hint_, LV_PART_MAIN);
  lv_obj_align(this->perm_hint_label_, LV_ALIGN_TOP_MID, 0, 36);
  lv_label_set_long_mode(this->perm_hint_label_, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(this->perm_hint_label_, 190);

  this->perm_approve_btn_ = lv_button_create(this->perm_card_);
  lv_obj_set_size(this->perm_approve_btn_, 90, 32);
  lv_obj_add_style(this->perm_approve_btn_, &this->style_perm_btn_approve_, LV_PART_MAIN);
  lv_obj_align(this->perm_approve_btn_, LV_ALIGN_BOTTOM_LEFT, 20, -12);
  lv_obj_t *approve_label = lv_label_create(this->perm_approve_btn_);
  lv_label_set_text(approve_label, "APPROVE1");
  lv_obj_center(approve_label);
  // Add an on-click callback handler
  lv_obj_add_event_cb(this->perm_approve_btn_, BuddyUI::on_approve_click_static, LV_EVENT_CLICKED, this);
  lv_group_add_obj(this->encoder_group_, this->perm_approve_btn_);
  // remove the container from the group and focus approve
  lv_group_remove_obj(this->container_);
  lv_group_focus_obj(this->perm_approve_btn_);

  this->perm_deny_btn_ = lv_button_create(this->perm_card_);
  lv_obj_set_size(this->perm_deny_btn_, 90, 32);
  lv_obj_add_style(this->perm_deny_btn_, &this->style_perm_btn_deny_, LV_PART_MAIN);
  lv_obj_align(this->perm_deny_btn_, LV_ALIGN_BOTTOM_RIGHT, -20, -12);
  lv_obj_t *deny_label = lv_label_create(this->perm_deny_btn_);
  lv_label_set_text(deny_label, "DENY1");
  lv_obj_center(deny_label);
  // Add an on-click callback handler
  lv_obj_add_event_cb(this->perm_deny_btn_, BuddyUI::on_deny_click_static, LV_EVENT_CLICKED, this);
  lv_group_add_obj(this->encoder_group_, this->perm_deny_btn_);
  
}

void BuddyUI::set_liveness(bool live) {
  if (live && this->state_ == State::DISCONNECTED) {
    this->transition_to_(State::IDLE);
  } else if (!live) {
    this->transition_to_(State::DISCONNECTED);
  }
}

void BuddyUI::push_status_message(const char *text) {
  if (this->state_ == State::PERMISSION || this->state_ == State::DISCONNECTED)
    return;
  if (text == nullptr || text[0] == '\0')
    return;
  if (strstr(text, "no message") != nullptr || strstr(text, "No message") != nullptr)
    return;

  lv_obj_t *label = this->arclabel_pool_[this->pool_write_ % kPoolSize];
  int16_t angle = rand() % 360;
  this->arrange_arclabel_(label, angle, text, false);
}

void BuddyUI::push_tool_message(const char *tool_name, const char *hint) {
  if (this->state_ == State::PERMISSION || this->state_ == State::DISCONNECTED)
    return;

  lv_obj_t *alabel = this->arclabel_pool_[this->pool_write_ % kPoolSize];
  int16_t angle = rand() % 360;
  this->arrange_arclabel_(alabel, angle, tool_name, true);
}

void BuddyUI::show_permission(const char *tool, const char *hint) {
  if (this->perm_card_ == nullptr)
    return;

  for (size_t i = 0; i < kPoolSize; i++) {
    if (this->arclabel_pool_[i] != nullptr) {
      lv_obj_set_style_opa(this->arclabel_pool_[i], LV_OPA_10, LV_PART_MAIN);
    }
  }

  lv_label_set_text(this->perm_tool_label_, tool);
  lv_label_set_text(this->perm_hint_label_, hint);
  lv_obj_clear_flag(this->perm_card_, LV_OBJ_FLAG_HIDDEN);
  this->permission_active_ = true;
  this->state_ = State::PERMISSION;
}

void BuddyUI::hide_permission() {
  if (this->perm_card_ == nullptr)
    return;

  lv_obj_add_flag(this->perm_card_, LV_OBJ_FLAG_HIDDEN);
  this->permission_active_ = false;
  this->state_ = State::IDLE;

  for (size_t i = 0; i < kPoolSize; i++) {
    if (this->arclabel_pool_[i] != nullptr) {
      lv_obj_set_style_opa(this->arclabel_pool_[i], LV_OPA_COVER, LV_PART_MAIN);
    }
  }
}

void BuddyUI::process_loop(uint32_t now_ms) {
  if (this->container_ == nullptr) return;

  for (size_t i = 0; i < kPoolSize; i++) {
    lv_obj_t *alabel = this->arclabel_pool_[i];
    if (alabel == nullptr || lv_obj_has_flag(alabel, LV_OBJ_FLAG_HIDDEN))
      continue;

    void *ud = lv_obj_get_user_data(alabel);
    if (ud == nullptr) continue;
    uintptr_t birth = (uintptr_t) ud;
    uint32_t age = now_ms - (uint32_t) birth;
    if (age > kMessageLifetime + kFadeInDuration) {
      if (lv_obj_get_style_opa(alabel, LV_PART_MAIN) <= LV_OPA_10) {
        lv_obj_add_flag(alabel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_opa(alabel, LV_OPA_TRANSP, LV_PART_MAIN);
      }
    }
  }
}

void BuddyUI::nudge_ring(int32_t delta) {
  if (this->state_ != State::IDLE)
    return;

  static int32_t accumulated = 0;
  accumulated += delta;
  if (accumulated > 12) accumulated = 12;
  if (accumulated < -12) accumulated = -12;

  lv_obj_set_style_transform_rotation(this->container_, accumulated * 10, LV_PART_MAIN);
}

static void buddy_anim_set_opa(void *obj, int32_t v) {
  lv_obj_set_style_opa((lv_obj_t *)obj, (lv_opa_t)v, LV_PART_MAIN);
}

void BuddyUI::arrange_arclabel_(lv_obj_t *label, int16_t angle, const char *text, bool is_tool) {
  float rad = angle * 3.14159f / 180.0f;
  int32_t x = 240 + (int32_t)(kMessageRadius * cosf(rad));
  int32_t y = 240 - (int32_t)(kMessageRadius * sinf(rad));

  lv_label_set_text(label, text);
  lv_obj_set_size(label, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_style_text_color(label, is_tool ? lv_color_hex(0xFF00AA) : lv_color_hex(0x00F0FF), LV_PART_MAIN);
  lv_obj_update_layout(label);
  lv_obj_set_pos(label, x - lv_obj_get_width(label) / 2, y - lv_obj_get_height(label) / 2);
  lv_obj_set_style_opa(label, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_clear_flag(label, LV_OBJ_FLAG_HIDDEN);

  lv_obj_set_user_data(label, (void *) (uintptr_t) millis());

  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, label);
  lv_anim_set_exec_cb(&a, buddy_anim_set_opa);
  lv_anim_set_values(&a, LV_OPA_TRANSP, LV_OPA_COVER);
  lv_anim_set_duration(&a, kFadeInDuration);
  lv_anim_start(&a);

  lv_anim_t a2;
  lv_anim_init(&a2);
  lv_anim_set_var(&a2, label);
  lv_anim_set_exec_cb(&a2, buddy_anim_set_opa);
  lv_anim_set_values(&a2, LV_OPA_COVER, LV_OPA_TRANSP);
  lv_anim_set_duration(&a2, kFadeOutDuration);
  lv_anim_set_delay(&a2, kMessageLifetime);
  lv_anim_start(&a2);

  this->pool_write_++;
}

void BuddyUI::transition_to_(State new_state) {
  if (this->state_ == new_state)
    return;

  this->state_ = new_state;

  switch (new_state) {
    case State::DISCONNECTED:
      if (this->center_pulse_) lv_obj_add_flag(this->center_pulse_, LV_OBJ_FLAG_HIDDEN);
      if (this->center_live_label_) lv_obj_add_flag(this->center_live_label_, LV_OBJ_FLAG_HIDDEN);
      if (this->center_no_signal_) lv_obj_clear_flag(this->center_no_signal_, LV_OBJ_FLAG_HIDDEN);
      if (this->perm_card_) lv_obj_add_flag(this->perm_card_, LV_OBJ_FLAG_HIDDEN);
      this->permission_active_ = false;
      for (size_t i = 0; i < kPoolSize; i++) {
        if (this->arclabel_pool_[i] != nullptr)
          lv_obj_add_flag(this->arclabel_pool_[i], LV_OBJ_FLAG_HIDDEN);
      }
      break;

    case State::IDLE:
      if (this->center_pulse_) lv_obj_clear_flag(this->center_pulse_, LV_OBJ_FLAG_HIDDEN);
      if (this->center_live_label_) lv_obj_clear_flag(this->center_live_label_, LV_OBJ_FLAG_HIDDEN);
      if (this->center_no_signal_) lv_obj_add_flag(this->center_no_signal_, LV_OBJ_FLAG_HIDDEN);
      if (this->perm_card_) lv_obj_add_flag(this->perm_card_, LV_OBJ_FLAG_HIDDEN);
      this->permission_active_ = false;
      break;

    case State::PERMISSION:
      break;
  }
}


void BuddyUI::on_approve_click_static(lv_event_t *event) {
  auto *ui = static_cast<BuddyUI*>(lv_event_get_user_data(event));
  if (ui != nullptr) {
    ui->OnApproveButtonClicked();
  }
}

void BuddyUI::on_deny_click_static(lv_event_t *event) {
  auto *ui = static_cast<BuddyUI*>(lv_event_get_user_data(event));
  if (ui != nullptr) {
    ui->OnDenyButtonClicked();
  }
}

void BuddyUI::OnApproveButtonClicked() {
    if (approve_action_) {
        approve_action_->play_complex(); // This safely triggers parent_->action_approve()
    }
}

void BuddyUI::OnDenyButtonClicked() {
    if (deny_action_) {
        deny_action_->play_complex(); // Triggers parent_->action_deny()
    }
}

}  // namespace ai_desktop_buddy
}  // namespace esphome
