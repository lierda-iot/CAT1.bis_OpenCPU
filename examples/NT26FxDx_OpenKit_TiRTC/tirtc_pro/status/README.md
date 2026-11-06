# Read-only system status

`tirtc_status_publish()` publishes a complete `tirtc_ui_system_t` through the
independent system mailbox. Call after the UI mailbox mutex exists, once before
`tirtc_status_start()`, then every five seconds in the application's main loop.
It never queries the modem or waits for an identity task. The identity task uses
a 4 KiB stack, priority 8, and retries missing values every five seconds; a
successful static identity value is cached for this boot.

## Real sources and limits

- Clock/date: `liot_rtc_get_localtime`, using the SDK's configured local timezone.
  SDK years are full years and months are 1–12. A failed read, a year outside
  2024–2100, or an invalid date/time clears the display to `--:--` / 时间未同步.
  Leap years and month lengths are checked. This validates the reading; it does
  not prove that an otherwise plausible RTC has recently synchronized. The
  adapter never sets the clock/timezone, starts NTP or assumes UTC+8.
- Module IMEI: `liot_dev_get_imei(..., 0)`, which calls the synchronous
  `appGetImeiNumSync` in the selected SDK. It executes only in the separate worker;
  even an indefinite modem wait leaves clock, volume and resources refreshes
  independent. Accept only a terminated, nonzero, 15-digit value. This is module
  identity, **not** the cloud application's device ID. Serial events do not
  contain the IMEI itself.
- Module firmware version: `liot_dev_get_firmware_version`, fetched before IMEI
  so a stuck modem identity request does not suppress a successful version read.
  The 256-byte SDK result is bounded; a value too long for the 64-byte UI field
  has a visible `...` suffix.
- Audio: `Liot_AudioGetVolume` returns the SDK's **software volume setting**,
  checked to be 0–100. The adapter reads the software setting; it does not
  initialize audio. It
  does **not** establish speaker/codec readiness or playback/recording state.
  The diagnostic explicitly labels the software value and states that recording
  and playback are not connected. `Liot_AudioGetCodecVolume` can call hardware
  and is deliberately not used. No audio, camera or peripheral is initialized.
- Events: actual caller/status changes with boot uptime, at most eight entries,
  bounded complete UTF-8 characters, adjacent duplicate suppression. Copying
  identity/events holds only the short UI mailbox lock; SDK calls, formatting,
  serial logging and publication happen outside it. Event API is task-only.
  Uptime follows the same F6D_D millisecond counter as resources; normal five
  second polling extends its 32-bit wrap. An old time sample captured just
  before another task's newer sample cannot create a false wrap.

The current build selects the official public headers and
`components/basePkg/F6D_D/lib/libliot_rtc.a`, `libliot_dev.a`,
`libliot_audio2.a` and `libliot_os.a`. The product's `board_compat`
directory no longer contains a second SDK/base package. RTC wrappers use
`OsaSystemTimeReadUtc` and `OsaTimerUtcToLocalTime`; they do not request network
time. This is verification of this bundled SDK, not a claim about future SDKs.

## UI interpretation and verification

This module only samples status; it does not enable or disable the product audio path. Its fallback text still contains “录音 / 播放：尚未接入”. AI publishes a separate audio diagnostic snapshot, which the UI uses when applicable. The fallback wording alone does not mean calls, LIVE or AI recording/playback are absent; verify the active business state and its media counters.

Current audio configuration and persistence are owned by media/preferences. `board_compat` selects the official `components/basePkg/$(MODEMPKG)` binaries; this product requires `MODEMPKG=F6D_D`. Identity, resource and status errors must stay visible without blocking UI/media tasks.

See [architecture](../docs/代码与流程详解.md) and the [build and firmware guide](../docs/构建与固件.md). Static/host checks cannot establish actual RTC accuracy, audio behavior or hardware stability.
