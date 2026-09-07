# demo_xiaozhi Debug Progress

Last updated: 2026-08-19 10:40 (Asia/Shanghai)

## Current status

The touch release / barge-in issue is **not resolved**. Pause Xiaozhi work here
and resume from this document after the other work is complete.

Latest test firmware:

`F:\cat1.bis_opencpu_internal\demo_xiaozhi_F9D_A_release_latch_stale_tts_fix_20260819.binpkg`

Latest relevant device log:

`C:\Users\001911\.codex\attachments\486faa90-b405-4371-9462-60b1c83784ee\pasted-text.txt`

## Implemented

- F9D_A current base package; 3A is not used because the API returned
  `L_AUD_ERR_NO_SUPPORT (11)`.
- Full-screen long press starts recording; release stops recording.
- Portrait GIF is embedded and fixed English status text is shown at the top.
- Audio capture uses a producer task plus an eight-block PCM queue.
- TTS is decoded and played as a stream; output and codec volume are 60.
- The first TTS block has 200 ms of leading silence for codec/PA warmup.
- Talk is enabled only after MCP `tools/list` has been answered.
- WebSocket sends an idle Ping every 20 seconds.
- RX semaphore is retained across reconnects to avoid cleanup deadlock.
- Barge-in sends `abort`, stops local audio and clears queued TTS.
- Zero-touch release reports are latched separately; the touch queue depth is 8.

## Remaining issue

Touch release / barge-in is still intermittent according to the latest hardware
test. The last analyzed log showed this concrete race:

1. User interrupted TTS at `10:37:03.975`.
2. The device sent abort, cleared TTS and entered RECORDING.
3. The old response sent `tts stop` at `10:37:04.098`.
4. That stale stop changed the new recording to COOLDOWN, so only five frames
   were uploaded; the physical release later reported state 2.

The current source attempts to ignore stale `tts stop` while RECORDING and to
latch zero-touch release independently, but the user reports that the overall
problem remains. These changes have not yet been validated as a complete fix.

## Resume investigation

- Add raw TP callback logs with sequence number, `touch_cnt`, event and queue
  result; correlate them with LVGL PRESS/LONG_PRESSED/RELEASED/PRESS_LOST.
- Give each listen/TTS exchange a local generation number. After abort, discard
  all TTS control and binary frames from the old generation until the server
  acknowledges abort or a new STT/TTS cycle begins.
- Do not let WebSocket RX callbacks directly overwrite RECORDING state.
- Verify whether BL6178 always emits `touch_cnt=0`; if not, also use the touch
  interrupt pin level or a bounded no-update release timeout.
- Re-test normal release, long hold, rapid repeated presses, Thinking barge-in,
  Speaking barge-in and reconnect followed by talk.

## Build

```powershell
.\build.bat build PROJECT=L_CT4IT02_1698W MODEM=NT26F9D0 BUILD_MODE=demo
```

Expected package:

`gccout\L_CT4IT02_1698W\L_CT4IT02_1698W_NT26F9D0_01.binpkg`
