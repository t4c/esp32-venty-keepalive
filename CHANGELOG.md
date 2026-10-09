# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.1] - 2026-10-09

### Added
- User configuration block at the top of the sketch:
  - `DISABLE_VIBRATION`: Toggle to automatically disable haptic vibration on BLE attach.
  - `KEEP_ALIVE_THRESHOLD_SECONDS`: Adjustable watchdog trigger threshold.
- Haptic vibration suppression command (`0x06` / `BRIGHTNESS_VIBRATION`) sent on device attach to prevent haptic buzzing during state bounces.

### Changed
- Default keep-alive trigger threshold adjusted from `<= 100s` to `<= 30s` (providing ~90 seconds of continuous uninterrupted session time between watchdog cycles).

## [1.0.0] - 2026-10-09

### Added
- Initial standalone autonomous daemon implementation for ESP32.
- BLE client auto-scanning and connection handling for Storz & Bickel VENTY (`S&B VY*`).
- Adaptive state-bouncing keep-alive logic across Normal (Mode 1), Boost (Mode 2), and Superboost (Mode 3).
- Background status polling and serial telemetry output.
- Automated GitHub Actions build workflow generating merged factory flashing binaries.
- Project documentation and flashing guides for browser web flashers, `esptool.py`, Arduino IDE, and PlatformIO.
