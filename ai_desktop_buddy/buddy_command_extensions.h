#pragma once

#include <cstdint>
#include <cstddef>
#include <span>

namespace esphome {
namespace ai_desktop_buddy {

class BuddyCommandExtensions {
 public:
  struct Sink {
    virtual void on_char_begin(const char *name, uint32_t manifest_size) = 0;
    virtual void on_file(const char *path, uint32_t size) = 0;
    virtual void on_chunk(std::span<const uint8_t> data) = 0;
    virtual void on_file_end() = 0;
    virtual void on_char_end() = 0;
  };

  void set_sink(Sink *sink) { this->sink_ = sink; }
  bool handle_command(const char *cmd, const char *line, size_t len);

 protected:
  Sink *sink_{nullptr};
};

}  // namespace ai_desktop_buddy
}  // namespace esphome
