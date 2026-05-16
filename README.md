# SynergyMill Lights

ESP32-based door sensor that posts Slack notifications when the side door is unlocked or locked.

## Hardware

- ESP32 development board
- Reed switch / door sensor on GPIO 15 (using internal pull-up)

## Features

- Sends Slack webhook messages on door state changes
- Debounced switch input (100ms) to prevent duplicate notifications
- Automatic WiFi reconnection
- OTA firmware updates via web interface with server-side authentication
- mDNS for easy network discovery

## Configuration

Edit the following constants in `lights.ino` before flashing:

| Constant | Description |
|----------|-------------|
| `host` | mDNS hostname |
| `ssid` | WiFi network name |
| `password` | WiFi password |
| `slackWebhook` | Slack incoming webhook URL |
| `otaUser` | OTA update login username |
| `otaPassword` | OTA update login password |

## OTA Updates

1. Navigate to `http://<host>.local/` in a browser
2. Log in with OTA credentials
3. Upload a compiled `.bin` firmware file

## Dependencies

- [ArduinoJson](https://github.com/bblanchon/ArduinoJson)
- ESP32 Arduino core (includes WiFi, WebServer, Update, mDNS)
