# Home Assistant Switch API Documentation (v2.0)

This document describes the HTTP API provided by the Home Assistant Sysmodule.

## General Information
- **Port**: 1337 (Default)
- **Protocol**: HTTP/1.1
- **Auth**: Requires `X-API-Token` header for all routes except `/info` and `/health`.

## Endpoints

### GET /info
Returns system information. No authentication required.
**Response**:
```json
{
  "version": "0.2.1",
  "battery": 85,
  "charging": true,
  "net": "Online"
}
```

### GET /screenshot
Captures a live screenshot of the Switch screen.
**Auth**: Required
**Response**: Binary `image/bmp` (1280x720, 24-bit).

### POST /button
Simulates controller inputs.
**Auth**: Required
**Payload**:
- Single button: `{"button": "A"}`
- Macro sequence: `{"sequence": [{"button": "A", "hold_ms": 100}, {"button": "WAIT", "hold_ms": 200}, {"button": "B"}]}`
- Joystick: `{"left_stick": {"x": 32767, "y": 0}}` (Range -32767 to 32767)

### POST /sleep
Puts the console into sleep mode.
**Auth**: Required

### POST /command
Executes advanced system-level actions.
**Auth**: Required
**Payload**:
- Reboot: `{"action": "reboot"}`
- Shutdown: `{"action": "shutdown"}`
- Launch Title: `{"action": "launch_app", "title_id": "0100000000010000"}`

### GET /logs
Returns the latest sysmodule logs in JSON format.
**Auth**: Required
