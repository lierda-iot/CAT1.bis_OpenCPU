# Audio preferences

The default is speaker volume **8**, microphone gain **8**, and both
speaker/microphone enabled. This module does not initialize or change audio
hardware. The application loads it before starting the UI, then applies the
configuration through its normal audio/UI controls.

## Call order

1. Call `tirtc_preferences_init()` once from the startup task before the UI.
   Return 0 means a valid saved record was loaded; 1 means defaults were used.
   Negative load errors leave `load_pending` set; `poll` retries unreadable slots
   before allowing a save, so unknown saved sequence numbers are not overwritten.
   Repeated initialization returns the cached result without reading again.
2. Read `tirtc_preferences_get()` for the initial configuration.
3. UI and hardware keys use `tirtc_preferences_update()` or
   `tirtc_preferences_adjust_volume()`. These perform one atomic RAM update;
   they do not access NVM, log, allocate memory or wait for a save.
4. One main/background task calls `tirtc_preferences_poll(now_ms)` every
   500–1000 ms (the current main loop uses 500 ms). The clock is monotonic **milliseconds**, including on the current F6D_D package.
   A real change must settle for 1000 ms before saving. An unchanged setting
   neither writes nor extends the debounce interval.
5. `tirtc_preferences_get_snapshot()` reports initialized/dirty/saving,
   the last save error, and the last verified sequence. A successful write
   can still leave dirty true if the UI changed again during NVM I/O.

The optional update result is the snapshot committed under the same lock.
The application must also serialize applying those snapshots to its UI/AI
mailboxes: an older caller must not apply its snapshot after a newer caller.
Never hold the UI/application lock while calling init or poll.

## On-disk format and recovery

The audio records are `tirtc_audio_a.nvm` and `tirtc_audio_b.nvm`.
Reads and writes use the official `liot_nvm_fread/fwrite` API; startup first
uses a read-only `LFS_stat` size check. That check depends on the pinned F6D_D
ABI and is covered by the build's base ELF/base API/NVM hash gates. When
changing the base package, review this dependency as well as the public API;
do not bypass the gates. Each payload is exactly 32 bytes:

| Offset | Field |
|---:|---|
| 0–3 | `TAUD` magic |
| 4 | Format version 1 |
| 5 | Record size 32 |
| 6–7 | Zero reserved bytes |
| 8–11 | Nonzero little-endian sequence |
| 12–15 | Volume, mic gain, speaker enabled, mic enabled |
| 16–27 | Zero reserved bytes |
| 28–31 | IEEE CRC32 over the preceding 28 bytes |

Levels must be 0..10 and switches exactly 0 or 1. The newest valid record
wins; a bad/torn record falls back to the other slot. Equal-sequence
disagreement or a half-range sequence ambiguity selects defaults, preserves
both files, and anchors the next requested save to slot A's sequence.
Defaults are not written automatically. The NVM driver may add its own
metadata beyond these 64 total payload bytes.

Save writes the alternate slot, reads it back, and verifies all 32 bytes
before advancing the active slot. A failed or uncertain write never reports
success. It remains dirty and retries no sooner than 5000 ms after that
poll attempt's timestamp. This includes a user reverting to the previously
saved value: an uncertain but complete newer slot must be overwritten with
the desired value before a reboot can safely select it.

No device-identity file, external Flash resource, or other user file is
touched. The module never formats storage or deletes a file. Full firmware
flashing that erases the underlying internal NVM/filesystem can still erase
these preferences; application-only updates should follow the project's
verified flashing instructions.

## Diagnostics and scope

`load_pending/load_error` report uncertain startup reads. User-edited fields win when a later retry recovers a valid old record. `value_revision` tracks RAM changes; `save_revision` advances only after a verified write, not after a load.

Save errors are -10 for failed/short write, -11 for failed/short readback, -12 for invalid/mismatching readback. They remain dirty and retry; an accepted UI action is not proof that flash saving has finished. Informational output is subject to the application log policy; do not require periodic debug logs for ordinary operation.

The four audio fields are persisted. Expression, screen-sleep choice, response-voice UI choice and current camera state are not stored by this module. Device identity uses its own record. A blank external Flash does not mean internal preferences were erased.

The call page's microphone toggle changes the same global microphone field
as Settings and is persisted through this module. Ending a call does not
automatically unmute it. The camera toggle only affects the current call.

See [settings and restart verification](../docs/当前设备操作.md#settings). Host fault injection is not a physical power-loss or NVM endurance test; release-specific evidence belongs to the release record.
