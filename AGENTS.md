# AI Desktop Buddy — ESPHome Component

An ESPHome port of the [Claude Desktop Buddy protocol](https://github.com/anthropics/claude-desktop-buddy) using Bluedroid BLE. Enables an ESP32 device to act as a physical companion for Claude Code — displaying session status, showing permission prompts, and providing hardware buttons for approve/deny without touching the keyboard.

Inspired by [esp-desktop-buddy](https://github.com/espressif/esp-desktop-buddy) (Espressif's ESP-IDF SDK, NimBLE-based). This component reimplements the same wire protocol for the ESPHome ecosystem using Bluedroid via `esp32_ble_server`.

## Architecture

```
ai-desktop-buddy/
  __init__.py                  # Python config schema, codegen, action registration
  ai_desktop_buddy.h / .cpp    # Top-level component — wires protocol ↔ transport
  buddy_protocol.h / .cpp      # JSON-line protocol parser (heartbeat, turns, commands)
  buddy_state.h                # Shared state structs (BuddyState, BuddyEvent union)
  buddy_transport_ble.h / .cpp # BLE NUS service, connection lifecycle, custom advertising
  buddy_command_extensions.h   # Optional char_* / folder-push sink (stub)
```

**Data flow**: BLE data → `BuddyTransportBLE::handle_rx_write_` → `BuddyProtocol::receive_bytes` → line parser → events (`SNAPSHOT_UPDATED`, `PERMISSION_RECEIVED`, `TURN`, etc.) → `AIDesktopBuddy::handle_protocol_event_` → ESPhome sensors + automations + LVGL widgets.

**TX path**: `action_approve/deny` → `BuddyProtocol::approve/deny` → JSON in TX queue → `BuddyTransportBLE::process_tx_queue_` → BLE notify.

## Sensors — automatically created

The component creates these entities automatically. All are prefixed with the component's name in Home Assistant.

### Numeric sensors

| Sensor | Meaning |
|--------|---------|
| `total_tasks` | All Claude sessions |
| `running_tasks` | Sessions actively generating |
| `waiting_tasks` | Sessions blocked on a permission prompt |
| `tokens` | Cumulative output tokens since app start |
| `tokens_today` | Output tokens since local midnight |

### Text sensors

| Sensor | Meaning |
|--------|---------|
| `status_message` | One-line summary from the heartbeat (`msg` field) |
| `prompt_data` | Current permission prompt as JSON (`{id, tool, hint}`) |
| `entries_data` | Recent transcript entries as a JSON array |
| `passkey` | Last BLE pairing passkey (6 digits, MITM mode only) |

### Binary sensors

| Sensor | Meaning |
|--------|---------|
| `liveness` | On while heartbeat is received (times out after `liveness_timeout`) |

## Actions

Two globally-registered actions for button bindings:

```yaml
on_click:
  - ai_desktop_buddy.approve:
      id: buddy

on_click:
  - ai_desktop_buddy.deny:
      id: buddy
```

`approve` sends `{"cmd":"permission","id":"...","decision":"once"}`. `deny` sends `"decision":"deny"`. Both are idempotent within a single prompt — only the first call per prompt ID takes effect.

## Automations / Event Hooks

```yaml
ai_desktop_buddy:
  on_turn:
    - logger.log:
        format: "[%s] %s"
        args: [role, content]
  on_permission:
    - logger.log:
        format: "Permission: %s — %s"
        args: [tool, hint]
  on_liveness:
    - logger.log:
        format: "Liveness: %d"
        args: [live]
  on_command:
    - logger.log:
        format: "Command: %s data=%s"
        args: [cmd, data]
```

| Hook | Args | Fires when |
|------|------|------------|
| `on_turn` | `role` (string), `content` (JSON string) | A turn completes (assistant response) |
| `on_permission` | `id` (string), `tool` (string), `hint` (string) | A permission prompt arrives |
| `on_liveness` | `live` (bool) | Connection liveness transitions |
| `on_command` | `cmd` (string), `data` (JSON string) | Unrecognised commands arrive |

## LVGL Widget Bindings

Bind textarea widgets for on-device display:

```yaml
ai_desktop_buddy:
  lvgl_widgets:
    status_msg: info_textarea    # shows msg field from heartbeat
    prompt_text: data_textarea   # shows hint text from permission prompts
    passkey: passkey_textarea    # shows pairing passkey (MITM mode only)
```

Each value is an `id` reference to an `lvgl` textarea widget. Widgets are resolved at codegen time and updated via `lv_textarea_set_text()`.

## Configuration Reference

```yaml
ai_desktop_buddy:
  id: buddy
  device_name: "Claude Buddy"      # optional, default "Claude Buddy"
  liveness_timeout: 30s            # optional, default 30s
  line_max: 2048                   # optional, 256–4096, default 2048
  tx_queue_depth: 8                # optional, 2–32, default 8
  lvgl_widgets:                    # optional
    status_msg: <textarea_id>
    prompt_text: <textarea_id>
    passkey: <textarea_id>
  on_turn: ...
  on_permission: ...
  on_liveness: ...
  on_command: ...
```

## Required YAML Setup

### BLE

```yaml
esp32_ble:
  io_capability: none        # "Just Works" pairing (no passkey)
  auth_req_mode: sc_bond     # LE Secure Connections + bonding
  advertising: true
  max_connections: 3
  use_psram: true            # recommended if your device has PSRAM
```

For **MITM pairing** (passkey display on-device), use `io_capability: display_only` and `auth_req_mode: sc_mitm_bond` instead.

### Platform

```yaml
esp32:
  board: esp32-s3-devkitc-1
  framework:
    type: esp-idf
```

ESP32-S3 with ESP-IDF is tested. ESP32 (original) with Arduino framework may work but is untested.

### Dependencies

```yaml
external_components:
  - source:
      type: local
      path: my_components/ai-desktop-buddy
    components: [ai_desktop_buddy]
```

The component auto-loads `json`, `sensor`, `text_sensor`, `binary_sensor`, and `esp32_ble_server`. You don't need to declare those explicitly. LVGL is optional — only needed if using `lvgl_widgets`.

## BLE Advertising & Auth Details

### Custom advertising

The component bypasses ESPHome's built-in advertising (`advertising_register_raw_advertisement_callback`) and calls the ESP-IDF gap APIs directly to stay within the 31-byte BLE advertisement payload. It advertises the 128-bit NUS service UUID in the advertisement packet and the device name + TX power in the scan response.

### Authentication flow

On connect, the component calls `esp_ble_set_encryption(bda, ESP_BLE_SEC_ENCRYPT)` to trigger pairing before the first GATT access. The Claude desktop app requires link encryption — it disconnects with HCI reason `0x13` (Remote User Terminated Connection) if the peripheral doesn't initiate encryption.

The component registers a GAP event callback to handle:
- `ESP_GAP_BLE_SEC_REQ_EVT` → accepts the security request
- `ESP_GAP_BLE_PASSKEY_NOTIF_EVT` → logs passkey + fires passkey callback (MITM only)
- `ESP_GAP_BLE_AUTH_CMPL_EVT` → tracks encrypted state

### Internals note

A `cg.add_define("ESPHOME_ESP32_BLE_GAP_EVENT_HANDLER_COUNT", 1)` in `__init__.py` is required because the component calls `add_gap_event_callback` directly in C++ (bypassing the normal Python registration mechanism). This controls the static callback array size in the ESP32BLE class.

## Protocol Reference

Full wire protocol specification: [claude-desktop-buddy REFERENCE.md](https://github.com/anthropics/claude-desktop-buddy/blob/main/REFERENCE.md)

Key details:
- Transport: BLE Nordic UART Service (`6e400001-...` / `6e400002-...` / `6e400003-...`)
- Wire format: newline-delimited UTF-8 JSON
- Heartbeat: every state change + 10s keepalive
- Commands: `status`, `name`, `owner`, `unpair`, `permission`
- Optional: folder push via `char_begin`/`file`/`chunk`/`file_end`/`char_end`
