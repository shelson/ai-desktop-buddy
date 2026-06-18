#pragma once

#include <cstdint>
#include <cstddef>
#include <array>
#include <string>
#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace ai_desktop_buddy {

#ifndef AI_BUDDY_LINE_MAX
#define AI_BUDDY_LINE_MAX 2048
#endif
#ifndef AI_BUDDY_ENTRY_COUNT_MAX
#define AI_BUDDY_ENTRY_COUNT_MAX 8
#endif
#ifndef AI_BUDDY_ENTRY_STRING_MAX
#define AI_BUDDY_ENTRY_STRING_MAX 128
#endif
#ifndef AI_BUDDY_MESSAGE_MAX
#define AI_BUDDY_MESSAGE_MAX 256
#endif
#ifndef AI_BUDDY_PROMPT_ID_MAX
#define AI_BUDDY_PROMPT_ID_MAX 64
#endif
#ifndef AI_BUDDY_PROMPT_TOOL_MAX
#define AI_BUDDY_PROMPT_TOOL_MAX 64
#endif
#ifndef AI_BUDDY_PROMPT_HINT_MAX
#define AI_BUDDY_PROMPT_HINT_MAX 128
#endif
#ifndef AI_BUDDY_TX_QUEUE_DEPTH
#define AI_BUDDY_TX_QUEUE_DEPTH 8
#endif
#ifndef AI_BUDDY_TURN_CONTENT_MAX
#define AI_BUDDY_TURN_CONTENT_MAX 4096
#endif

struct PromptData {
  bool present{false};
  char id[AI_BUDDY_PROMPT_ID_MAX + 1]{};
  char tool[AI_BUDDY_PROMPT_TOOL_MAX + 1]{};
  char hint[AI_BUDDY_PROMPT_HINT_MAX + 1]{};
};

struct BuddyState {
  bool has_state{false};
  uint32_t total{0};
  uint32_t running{0};
  uint32_t waiting{0};
  uint64_t tokens{0};
  uint64_t tokens_today{0};
  char msg[AI_BUDDY_MESSAGE_MAX + 1]{};
  size_t entry_count{0};
  std::array<char, AI_BUDDY_ENTRY_COUNT_MAX * (AI_BUDDY_ENTRY_STRING_MAX + 1)> entries_{};
  PromptData prompt;
};

enum class BuddyEventType : uint8_t {
  SNAPSHOT_UPDATED = 0,
  PERMISSION_RECEIVED,
  TIME_SYNC,
  TURN,
  LIVENESS_CHANGED,
  COMMAND,
  COMMAND_EXTENSION,
  ERROR,
};

struct PermissionEvent {
  char id[AI_BUDDY_PROMPT_ID_MAX + 1];
  char tool[AI_BUDDY_PROMPT_TOOL_MAX + 1];
  char hint[AI_BUDDY_PROMPT_HINT_MAX + 1];
};

struct TimeSyncEvent {
  int64_t epoch;
  int32_t tz_offset;
};

struct TurnEvent {
  char role[16];
  char content[AI_BUDDY_TURN_CONTENT_MAX + 1];
};

struct CommandEvent {
  char cmd[64];
  char data[AI_BUDDY_TURN_CONTENT_MAX + 1];
};

struct ErrorEvent {
  uint8_t kind;
  uint32_t detail;
};

struct BuddyEvent {
  BuddyEventType type;
  union {
    PermissionEvent permission;
    TimeSyncEvent time_sync;
    TurnEvent turn;
    CommandEvent command;
    ErrorEvent error;
    bool live;
  };
};

struct TxFrame {
  uint8_t data[AI_BUDDY_LINE_MAX + 2];  // JSON + \n + \0
  size_t len;
};

enum class TxReadyState : uint8_t {
  NOT_READY = 0,
  READY,
};

}  // namespace ai_desktop_buddy
}  // namespace esphome
