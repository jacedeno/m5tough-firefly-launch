# Firefly Launch Display

A desk display for the **M5Stack M5Tough** that shows the next **Firefly Aerospace** rocket launch with a live T-minus countdown, styled in Firefly's brand green. Launch data comes from [The Space Devs Launch Library 2](https://thespacedevs.com/llapi); weather from [Open-Meteo](https://open-meteo.com/).

The screen alternates between three views every 8 seconds, and you can tap the touchscreen to switch instantly:

- **Mission Control** — telemetry-style layout: big countdown plus mission, rocket, pad, site and orbit.
- **Hero** — the mission image as a full-color background with the mission name and countdown overlaid.
- **Clock** — Firefly logo, a large 24-hour clock (US Central time), the date, and current weather for Briggs, TX (Firefly's test site).

## Hardware

- **M5Stack M5Tough** (Core2-based ESP32, 16 MB flash, PSRAM, 320×240 capacitive touchscreen)
- USB-C cable for flashing

No wiring required — the M5Tough is self-contained (display, touch, battery, Wi-Fi).

## Build & flash

This project uses [PlatformIO](https://platformio.org/).

```bash
pio run                # build
pio run -t upload      # build + flash over USB
pio device monitor     # view serial logs (115200 baud)
```

The board is configured as `m5stack-core2`; [M5Unified](https://github.com/m5stack/M5Unified) auto-detects the M5Tough at runtime.

## Wi-Fi setup

Credentials are managed with [WiFiManager](https://github.com/tzapu/WiFiManager) — nothing is hard-coded, so the repo stays clean and the device is portable between networks.

1. On first boot (or when the saved network isn't found), the device starts its own access point: **`Firefly-Display-Setup`**.
2. Connect to it from your phone; a captive portal opens (or browse to `192.168.4.1`).
3. Pick your network and enter the password. The device reboots and connects.

**To change networks** (e.g. moving from home to the office): **hold a finger on the screen while powering on** to wipe the saved credentials and reopen the setup portal.

### Optional: preset credentials

To skip the portal (handy on a network without a captive portal), create `src/secrets.h` — it is gitignored and never committed:

```cpp
#pragma once
#define WIFI_SSID "your-network"
#define WIFI_PASS "your-password"
```

When present, the device connects directly to that network and retries until it succeeds. Delete the file to fall back to the WiFiManager portal.

## How it works

- On boot the device connects to Wi-Fi, syncs the clock via NTP (UTC), and fetches the next Firefly launch.
- Launch data is refreshed every 2 hours (the API has a low anonymous rate limit, so polling is intentionally infrequent). The countdown ticks every second from the local clock, so it stays smooth between refreshes.
- The mission image is fetched through [images.weserv.nl](https://images.weserv.nl/), a free image proxy that re-encodes it to a **baseline** JPEG and resizes it to the screen. This is required because the display's JPEG decoder cannot handle the progressive JPEGs the launch API serves.

Because Firefly launches are infrequent, the next launch is usually weeks or months out and may show a tentative (`TBD`) date — so the countdown is shown in days/hours rather than a dramatic seconds count.

## Configuration

Key constants live at the top of [`src/main.cpp`](src/main.cpp):

| Constant | Purpose |
|----------|---------|
| `LL2_URL` | API query (change `search=Firefly` to track a different provider) |
| `FETCH_INTERVAL_MS` | How often launch data is refreshed (default 2 h) |
| `VIEW_SWITCH_MS` | Auto-rotate interval between views (default 8 s) |
| `AP_NAME` | Wi-Fi setup access point name |

## Data & credits

Launch data provided by [The Space Devs](https://thespacedevs.com/) via the Launch Library 2 API. This is an independent hobby project and is not affiliated with Firefly Aerospace or NASA.
