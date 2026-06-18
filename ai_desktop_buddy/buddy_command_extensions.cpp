#include "buddy_command_extensions.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ai_desktop_buddy {

static const char *const TAG = "buddy_cmd_ext";

bool BuddyCommandExtensions::handle_command(const char *cmd, const char *line, size_t len) {
  if (this->sink_ == nullptr)
    return false;

  if (strcmp(cmd, "char_begin") == 0) {
    this->sink_->on_char_begin("", 0);
    return true;
  }
  if (strcmp(cmd, "char_end") == 0) {
    this->sink_->on_char_end();
    return true;
  }
  return false;
}

}  // namespace ai_desktop_buddy
}  // namespace esphome
