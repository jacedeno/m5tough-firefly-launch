<h1 align="center">Firefly Launch Display</h1>
<p align="center"><em>Firefly Aerospace next-launch desk display for M5Stack M5Tough.</em></p>

<p align="center">
  <a href="#overview"><img src="https://img.shields.io/badge/views-3%2C%20auto--rotating-1f6feb?style=flat-square" alt="Views: 3, auto-rotating"></a>
  <a href="#overview"><img src="https://img.shields.io/badge/touch-tap%20to%20switch%20views-6e7781?style=flat-square" alt="Touch: tap to switch views"></a>
  <a href="#hardware"><img src="https://img.shields.io/badge/wiring-none%20required-2ea44f?style=flat-square" alt="Wiring: none required"></a>
</p>
<p align="center">
  <img src="https://img.shields.io/badge/M5Stack-M5Tough-2C3E50?style=flat-square" alt="M5Stack M5Tough">
  <img src="https://img.shields.io/badge/ESP32-Core2-E7352C?style=flat-square&logo=espressif&logoColor=white" alt="ESP32">
  <img src="https://img.shields.io/badge/PlatformIO-build-F5822A?style=flat-square&logo=platformio&logoColor=white" alt="PlatformIO">
  <img src="https://img.shields.io/badge/C%2B%2B-Arduino-00599C?style=flat-square&logo=cplusplus&logoColor=white" alt="C++">
  <img src="https://img.shields.io/badge/Launch%20Library%202-The%20Space%20Devs-6e7781?style=flat-square" alt="Launch Library 2 API">
  <img src="https://img.shields.io/badge/Open--Meteo-weather-6e7781?style=flat-square" alt="Open-Meteo">
  <a href="LICENSE"><img src="https://img.shields.io/github/license/jacedeno/m5tough-firefly-launch?style=flat-square" alt="License"></a>
  <img src="https://img.shields.io/github/last-commit/jacedeno/m5tough-firefly-launch?style=flat-square" alt="Last commit">
</p>

## Overview

A desk display for the **M5Stack M5Tough** that shows the next **Firefly Aerospace** rocket launch with a live T-minus countdown, styled in Firefly's brand green. Launch data comes from [The Space Devs Launch Library 2](https://thespacedevs.com/llapi); weather from [Open-Meteo](https://open-meteo.com/).

The screen alternates between three views every 8 seconds, and you can tap the touchscreen to switch instantly:

- **Mission Control** — telemetry-style layout: big countdown plus mission, rocket, pad, site and orbit.
- **Hero** — the mission image as a full-color background with the mission name and countdown overlaid.
- **Clock** — Firefly logo, a large 24-hour clock (US Central time), the date, and current weather for Briggs, TX (Firefly's test site).

## Features

### How it works

- On boot the device connects to Wi-Fi, syncs the clock via NTP (UTC), and fetches the next Firefly launch.
- Launch data is refreshed every 30 minutes (the API has a low anonymous rate limit, so polling is intentionally infrequent). The countdown ticks every second from the local clock, so it stays smooth between refreshes.
- The mission image is fetched through [images.weserv.nl](https://images.weserv.nl/), a free image proxy that re-encodes it to a **baseline** JPEG and resizes it to the screen. This is required because the display's JPEG decoder cannot handle the progressive JPEGs the launch API serves.

Because Firefly launches are infrequent, the next launch is usually weeks or months out and may show a tentative (`TBD`) date — so the countdown is shown in days/hours rather than a dramatic seconds count.

### Hardware

- **M5Stack M5Tough** (Core2-based ESP32, 16 MB flash, PSRAM, 320×240 capacitive touchscreen)
- USB-C cable for flashing

No wiring required — the M5Tough is self-contained (display, touch, battery, Wi-Fi).

## Getting started

### Build & flash

This project uses [PlatformIO](https://platformio.org/).

```bash
pio run                # build
pio run -t upload      # build + flash over USB
pio device monitor     # view serial logs (115200 baud)
```

The board is configured as `m5stack-core2`; [M5Unified](https://github.com/m5stack/M5Unified) auto-detects the M5Tough at runtime.

### Wi-Fi setup

Credentials are managed with [WiFiManager](https://github.com/tzapu/WiFiManager) — nothing is hard-coded, so the repo stays clean and the device is portable between networks.

1. On first boot (or when the saved network isn't found), the device starts its own access point: **`Firefly-Display-Setup`**.
2. Connect to it from your phone; a captive portal opens (or browse to `192.168.4.1`).
3. Pick your network and enter the password. The device reboots and connects.

**To change networks** (e.g. moving from home to the office): **hold a finger on the screen while powering on** to wipe the saved credentials and reopen the setup portal.

#### Optional: preset credentials

To skip the portal (handy on a network without a captive portal), create `src/secrets.h` — it is gitignored and never committed:

```cpp
#pragma once
#define WIFI_SSID "your-network"
#define WIFI_PASS "your-password"
```

When present, the device connects directly to that network and retries until it succeeds. Delete the file to fall back to the WiFiManager portal.

### Configuration

Key constants live at the top of [`src/main.cpp`](src/main.cpp):

| Constant | Purpose |
|----------|---------|
| `LL2_URL` | API query (change `search=Firefly` to track a different provider) |
| `FETCH_INTERVAL_MS` | How often launch data is refreshed (default 30 min) |
| `VIEW_SWITCH_MS` | Auto-rotate interval between views (default 8 s) |
| `AP_NAME` | Wi-Fi setup access point name |

## Data & credits

Launch data provided by [The Space Devs](https://thespacedevs.com/) via the Launch Library 2 API. This is an independent hobby project and is not affiliated with Firefly Aerospace or NASA.

## License

MIT — see [LICENSE](LICENSE).

---

<p align="center">
  Built by <b>Jose Cedeno</b> ·
  <a href="https://geekendzone.com">geekendzone.com</a> ·
  <a href="https://www.linkedin.com/in/joseangelcedeno/">LinkedIn</a>
</p>
