# V04 external storage

This module owns SPI1 / P25Q32SH and an independent LittleFS instance. It never
calls `liot_finit_ext`, which the historically audited F6D_A binary implements as mount,
format on failure, then mount again. The current build uses the public F6D_D SDK;
F6D_A addresses and behavior cited below describe that historical audit, not
addresses to reuse with F6D_D. This module exposes no public format function.

## Runtime recovery

Read-side I/O faults on a previously mounted volume now retry after 5/10/20/40/60 seconds. Each attempt only resets the SPI transport, verifies the original JEDEC identity twice, mounts the original geometry and samples usage. Recovery never formats, grows or replays a write. Initial mount failures, identity/layout changes and mutating-operation failures remain unavailable.

A media fault invalidates all open descriptors and discards unsynced buffers without I/O; reopen explicitly after recovery. Descriptors are opaque, non-reused positive tokens. An old token cannot access a newly opened file. Only successful sync/close confirms buffered data.

`[flash12]` diagnostics separate whole-operation budget, FIFO receive/BUSY, NOR BUSY and recovery results. FIFO timeout decisions recheck hardware after task preemption; genuine stalls and a frozen tick still terminate. The 10-second whole-operation wall-clock limit remains bounded, and its expiry is reported honestly.


## Integration

The production core compiles `tirtc_storage.c`, `tirtc_storage_port.c`, `tirtc_spi_fifo.c`, and
`tirtc_lfs.c`. Do not separately compile the `littlefs/` C files: the last file
includes them with `ts_lfs_*` symbol names. Include `tirtc_storage.h` in callers.

The project also compiles `examples/tirtc_storage_example.c`, an append/sync/read-back example. It is not invoked at startup; call it explicitly from a background business task.

After the display port has enabled and settled the shared 3.3 V supply, call
`tirtc_storage_init()` once in a dedicated background task, then
`tirtc_storage_poll()` every 5 seconds. An 8 KiB task stack is configured by the
application. There is no storage-created task or whole-chip RAM buffer.

All filesystem calls share one mutex (100 ms acquisition limit; poll tries
without waiting). Snapshot reads are task-only and use the paired, nestable
FreeRTOS task critical section around a structure copy, independent of that
mutex. The F6D_A `_from_isr` critical wrapper is a no-op for an ordinary task;
the adapter uses `liot_rtos_enter_critical` / `liot_rtos_exit_critical` instead. No SPI,
file I/O or mutex wait occurs in this critical section. `filesystem_sample_ms`
changes only when a filesystem usage sample completes, including a failed
sample. `progress_ms` advances during actual scan/validation progress. A skipped
poll does not make old data appear fresh.

The public API supports two simultaneous files using fixed 256-byte caches.
Read/write return bytes; transfer at most 4096 bytes per call. File operations
belong in background business tasks, never LVGL callbacks. `r`, `w`, `a`, `r+`,
`w+`, `a+` and the binary variants are accepted. Create parent directories first.
Do not run the old SDK external-flash demo or another SPI1 driver concurrently.

## Identification and preservation

Two `9F` reads must agree. Only JEDEC `85 60 16` (Puya P25Q32SH, 4 MiB) enables
this write implementation. Other plausible capacities are reported but the chip
is left untouched. All reads, erases and writes are checked against its capacity.

| Existing contents | Action |
| --- | --- |
| Valid 4 KiB-block, 4 MiB LittleFS 2.0 | Mount and report actual usage |
| Valid 4 KiB-block, 2 MiB LittleFS 2.0; upper half all FF | `lfs_fs_grow` transactionally increases to 4 MiB and retains files |
| Valid 2 MiB filesystem; upper half has data | Keep the original filesystem; `READY_LIMITED` |
| Entire chip all FF, two complete successful passes | Reconfirm ID, create a 4 MiB LittleFS 2.0 filesystem |
| SDK legacy prebuilt geometry mismatch | Use the legacy view read-only only when it is the sole fully validated view |
| Genuine 256-byte logical-block image | Use the native view read-only only when it is the sole fully validated view |
| Both native and legacy views validate | Report unknown data; do not guess which view contains the latest files |
| Unknown nonblank, read failure, unsupported chip | No automatic format or erase; publish the error/state |

Data operations never clear the device's protection bits. Programming and 4 KiB
erase verify their results by reading back. Only an entirely blank device is
formatted; no format fallback follows a corrupt or failed mount.

### Actual SDK prebuilt image

The historical SDK prebuilt `extflashlfs/ext_lfs.bin` audited for this compatibility path was 2 MiB. This file is not a required resource in the current public `components` tree. Its stored
superblock claims 256-byte blocks / 8192 blocks, but its real layout is 4096-byte
blocks / 512 blocks. SDK `lfsutil -B 4096` reads eight files and 96 used blocks;
using `-B 256` cannot read their file chains correctly.

The sole vendor patch is a hook after normal LittleFS metadata CRC validation.
During a dedicated read-only fallback only, it normalizes the exact 256/8192
claim to 4096/512 **in memory**. It does not rewrite the superblock, relocate
files, grow this filesystem, or erase neighbouring bytes. All file chains are
read and checked before declaring it usable. The adapter evaluates both native and legacy read-only views. A valid-looking legacy view can expose a stale empty file from a genuinely native image, so two valid candidates are rejected as ambiguous instead of selecting one silently. The historical comparison checked extracted file contents with the SDK utility; this is provenance for the read-only compatibility path, not a new run against the current device.
Unknown layouts fail closed. A future writable migration requires a separate
data-preserving migration design; it is intentionally not implicit here.

LittleFS is pinned to v2.9.3 / commit
`d01280e64934a09ba16cac60cf9d3a37e228bb66`. `LFS_MULTIVERSION` keeps new and grown
filesystems in the SDK's disk version 2.0. The upstream BSD-3-Clause license is included in `littlefs/LICENSE.md`.

## Bounded hardware transport and evidence

Pins: SPI1 MOSI PAD63, MISO PAD62, SCLK PAD49, manual CS PAD64 / GPIO12. SDK
`liot_spi_init_ext` sets 8-bit mode 0 / 6.5 MHz / no automatic CS. It does not
toggle the shared display supply. Each command keeps manual CS low for its
complete command/address/data transaction.

The SDK's `liot_spi_write_read` outer status poll has a limit, but its synchronous
`SPI_Transfer` contains an unlimited FIFO loop. The bounded replacement uses the
same SPI B2 register sequence documented in `ec718.h` and present in the actual
F6D_A `ap_lierda_app.elf` `SPI_Transfer` at `0x008d67c8`:

- CR1 bit 1 enables the controller; SR bit 2 indicates received data, bit 4 busy,
  bits 16..23 count queued transmit bytes.
- CER=3, WBSR=1/3/7/15, RBSR, RBLR and RCCR implement packed 8-bit transfers.
- Interrupts and DMA requests are disabled for this exclusive polling owner.
- FIFOs/counters are cleared before and after a transfer. CS is released and
  the controller disabled on success, timeout, bad receive strobe or CS error.
- FIFO waits have a 20 ms no-progress limit and 1,000,000-iteration ceiling,
  including when the tick counter stops. UI/network preemption after actual
  transfer progress does not falsely report timeout. A completed transfer with
  BSY clear succeeds even if it was just preempted. NOR busy polling yields and
  has a 3-second limit.
- Overall initialization has a 120-second I/O budget; later operations have a
  10-second budget checked at each hardware transaction. Every 4 KiB of blank
  scanning yields 1 ms and publishes progress.

Register definitions are from the actual bundled SDK, not guessed addresses.
The public SDK initialization and GPIO calls still have their own platform
implementation. Host tests verify the FIFO control flow and fault cleanup, not
real electrical timing, clock/power recovery or the installed chip's health.
**No hardware erase, initialization or flashing was performed by these tests.**

Official chip specification: [P25Q32SH V1.8, JEDEC table page 79](https://www.puyasemi.com/download_path/%E6%95%B0%E6%8D%AE%E6%89%8B%E5%86%8C/Flash/P25Q32SH_Datasheet_V1.8.pdf).


## Current application use

UI fonts and background are loaded through this filesystem; install instructions are in [resource_store](../resource_store/README.md). State values are defined in [tirtc_storage.h](tirtc_storage.h): OFF=0, PROBING=1, READY=2, READY_LIMITED=3, ABSENT=4, UNSUPPORTED=5, UNKNOWN_DATA=6, ERROR=7. `read_only` is a separate flag; the UI may map it to a display-only READ_ONLY state.

Audio preferences and platform identity use internal NVM, not this volume. External Flash stores files; it is not additional executable code space or video-frame RAM. Error interpretation and data-preservation behavior are summarized in [operation/storage errors](../docs/当前设备操作.md#storage-errors).
