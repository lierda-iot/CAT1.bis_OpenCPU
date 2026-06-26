# Changelog

All notable changes to this project will be documented in this file.

---

## Versioning Convention

Uses **extended semantic versioning** format: `MAJOR.MINOR.PATCH`

- **MAJOR**: Major updates with significant structural and API changes. Release cycle: yearly
  - Example: v1.0 → v2.0

- **MINOR**: Backward-compatible feature additions (new modules, drivers, demos). Release cycle: monthly
  - Example: v1.0 → v1.1

- **PATCH**: Bug fixes and performance optimizations. Released as needed (weekly or faster)
  - Extended format: v1.1 → v1.1.patch01 → v1.1.patch02

**Pre-release versions**: Use `-beta` suffix

- Example: `v1.2-beta`, `v1.3-beta`

---

## Change Types

Each version entry should include the following categories (as applicable):

| Type           | Description                    | Usage                                                          |
| -------------- | ------------------------------ | -------------------------------------------------------------- |
| **Added**      | New features                   | New module support, drivers, APIs, example code                |
| **Changed**    | Changes to existing features   | API optimization, performance improvements, config adjustments |
| **Deprecated** | Soon-to-be removed features    | APIs or features marked as deprecated but still functional     |
| **Removed**    | Removed features               | Completely removed APIs, drivers, or modules                   |
| **Fixed**      | Bug fixes                      | Functional defects, stability issues, memory leaks             |
| **Security**   | Security-related               | Vulnerability fixes, security library upgrades                 |

---

<!-- CHANGELOG_PLACEHOLDER -->

## [v1.5.patch03] - 2026-06-26

### Added

- Add models NT26F9D0 and F9D_A
- Add ft6336 support for touch panel driver
- add apnv_recv_part api
- support float add double

### Changed

- Optimize sound sample code
- Optimize demo for L_CT4IT00_YP00W_01_V04
- Optimize sound sample code
- Enable WebSocket compilation for CT4IT00YP00W_03
- Optimize app Makefile for more general use

### Fixed

- Fix tts Makefile parameter passing

## [v1.5] - 2026-06-17

### Added

- add xiaodacall with CT4IT00YP00W_03
- Add exflash, exflashfs, and lvgl interfaces
- add apnv_recv_part api
- support float add double

### Changed

- repet sound api as audio api
- open source websocketlvgl/opus/amr/mp3

### Fixed

- update gx8006 driver

## [v1.4] - 2026-06-05

### Added

- Add SDK_VERSION management to independently track SDK and APP versions
- Add PhonePe API and demo
- Add audio volume control API, MP3 streaming playback API, WAV playback API
- Add AXS5106 and CST816D touch panel drivers
- Add ST77916 QSPI display driver
- Add external flash interface
- Add external flash filesystem interface
- Add external flash direct access demo
- Add GX8006 driver interface
- Add battery management driver, WS2812B driver framework

### Changed

- Optimize liot_i2c.h declarations
- Optimize liot_lcd interface
- Optimize README
- Docs directory maintained in separate documentation repository

### Fixed

- Fix filesystem end address calculation

---

## [v1.3] - 2026-05-25

### Added

- Add build support for NT26F6D0_GL module
- Add commit convention documentation

### Changed

- Optimize MODEM model matching logic, replace findstring with filter

### Fixed

- Remove debug log in key interrupt handler (L-CS4BF01-YW-0950)
- Fix missing NT26FCNB60WNA IO pin configuration for F6B_A model

---

## [v1.2] - 2026-05-14

### Added

- Add LVGL support and example code

### Changed

- Update documentation and README
- Optimize GPIO `Liot_WakeupIntInit` interface parameters

---

## [v1.1] - 2026-05-11

### Added

- **Initial release**: Lierda LTE-EC71X OpenCPU SDK v1.1
- **Supported modules**: F6B_A, F6D_A, F7B_A, K2B_A, K2F_A series
- **Peripheral drivers**: UART, GPIO, I2C, SPI, ADC, PWM, Timer, USB, Camera, LCD
- **Middleware**: FOTA OTA upgrade, filesystem management, TTS, AT command framework, network stack (Socket, HTTP, FTP, MQTT)
- **Third-party libraries**: cJSON, FreeRTOS, mbedTLS, lwIP, libwebsockets
- **Examples**: `examples/app` (empty template), `examples/demo` (API usage demos)
- **Documentation**: Getting Started Guide, Data Dial Guide, Device Management Guide, Low Power Mode Guide, Full OTA Upgrade Guide, Firmware Flash Tool Guide

### Changed

- **Toolchain**: ARM GCC 10-2020-q4-major

---
