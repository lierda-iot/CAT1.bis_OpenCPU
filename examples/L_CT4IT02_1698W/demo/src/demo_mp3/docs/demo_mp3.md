# MP3 播放器 Demo（L_CT4IT02_1698W）

基于 LVGL v8.4.0 横屏显示和触摸方案，实现 MP3 播放器界面、专辑封面显示、播放/暂停控制和进度条演示。

更新时间：2026-10-10

## 功能

- 开机自动播放 MP3 歌曲（Because Of You，60 秒）
- 专辑封面 360x360 全屏显示
- 播放/暂停按钮，触摸控制
- 进度条和时间显示（当前时间 / 总时长）
- 首次播放自动 autokick（pause/resume）解决 ES8375 冷启动无声问题

## 设计背景

本 Demo 用于 L_CT4IT02_1698W 圆屏样机展示 MP3 音频播放能力，同时验证以下功能链路：

- LVGL 横屏 UI 与触摸交互
- 外部 SPI Flash 文件系统（P25Q64）读写
- ES8375 I2S MP3 解码播放
- `.incbin` 嵌入资源 + extflash 文件系统双路径写入

## 构建与烧录

### 构建命令

```bash
make all PROJECT=L_CT4IT02_1698W MODEM=NT26F9D0 MODEMPKG=F9D_A BUILD_MODE=demo HWDEMO_MP3_EN=y
```

### 烧录文件

```text
gccout/L_CT4IT02_1698W/L_CT4IT02_1698W_NT26F9D0_01.binpkg
```

### 编译开关说明

- `HWDEMO_MP3_EN=y`：编译 `demo_mp3_stream` / `demo_mp3_core` / `demo_mp3_resources`，并拉起 LVGL 与 GSENSOR 依赖，入口线程 `liot_mp3_demo_thread`。
- `HWDEMO_SOUND_ES8375_EN` 默认 n：板载声音 demo 的三选一默认关闭，无需显式设置。
- `APP_MP3_EN` 只作用于 `BUILD_MODE=app`；demo 模式下 MP3 仅由显式 `HWDEMO_MP3_EN=y` 或 `HWDEMO_SALARY_CALCULATOR_EN=y` 联动打开，两者无隐式联动。
- `HWDEMO_SALARY_CALCULATOR_EN=y` 会自动联动 `HWDEMO_MP3_EN=y` + `BUILD_THPART_MP3_ENABLE=y`，因为金币音效依赖 `demo_mp3_resources.c` 内嵌的 `g_salary_coin_mp3_start/end`（`coin_burst_44k.mp3`）。只验证播放器时可保持 salary 关闭。

## 资源文件

资源文件位于本目录：

- `because of you.mp3`：MP3 原始音源（用户提供，保留不修改）
- `because of you_-12dB.mp3`：实际内嵌 MP3 音源，CBR 56 kbps，44.1 kHz，mono，无 ID3 标签
- `because of you.jpg`：用户提供的原始封面图，保留不修改
- `because_of_you_cover_360x360.rgb565`：360x360 RGB565 原始图像资源
- `demo_mp3_resources.c`：通过 `.incbin` 将 MP3 和封面嵌入固件

首次烧录时 MP3 和封面会自动写入外部 SPI Flash 文件系统：

- `/flash/mp3/because_of_you.mp3`
- `/flash/mp3/because_of_you_cover_360x360.rgb565`

本目录同时存放被其他 demo 共享的内嵌资源：

- `sine_1khz_16k_mono_s16le.pcm`：1kHz tone demo 的内嵌 PCM 音源
- `coin_burst_44k.mp3`：salary 金币音效的 44.1kHz MP3 内嵌资源（由 `demo_mp3_resources.c` 导出 `g_salary_coin_mp3_start/end`）

> 注意：`demo_mp3_resources.c` 使用 `.incbin` 引用上述资源，更换音源后需删除旧的 `demo_mp3_resources.o` 再编译，否则 Makefile 不会自动感知资源文件变化。

## 操作与预期

1. 烧录后开机，等待搜网完成（约 8 秒）。
2. 自动进入播放器页面，显示封面和歌曲信息。
3. 自动开始播放 Because Of You。
4. 触摸播放/暂停按钮可暂停和恢复播放。
5. 播放完成后自动停止，可手动再次播放。

## 实测日志（真机 2026-08-31）

应用层关键日志：

```text
[demo_mp3_stream] StreamStart ret=0
[demo_mp3_stream] STATE STOPPED -> PLAYING (mp3_playback_task)
[demo_mp3_stream] Pause ret=0
[demo_mp3_stream] STATE PLAYING -> PAUSED (mp3_pause_play)
[demo_mp3_stream] Resume ret=0
[demo_mp3_stream] STATE PAUSED -> PLAYING (mp3_playback_task)
[demo_mp3_stream] EVT_START state=PLAYING
[demo_mp3_stream] EVT_FINISH state=PLAYING
[demo_mp3_stream] STATE PLAYING -> STOPPED (mp3_playback_task)
```

底层音频解码/播放关键日志：

```text
AudMp3Decode nchannels / 1 samplerate / 44100 datalen / 2304 	2026-08-31 09:27:43.244
aud start play 	2026-08-31 09:27:43.250
aud play audio 0xc186984 ( len = 2304 ) 	2026-08-31 09:27:43.250
aud stop play 	2026-08-31 09:27:43.739
```

总时长/进度操作（Nitz 校时后 UTC+8 显示）实测播放从 `18:21:45.181` 持续至 `18:22:36.639`，期间包含多次 Pause/Resume 与进度条总时长查询操作，确认 UI 进度时间显示链路可用。

## 验证状态

- 编译已验证（demo 组合，`HWDEMO_MP3_EN=y`）。
- 真机播放/暂停/恢复与进度链路已验证，见上文实测日志。
- 纯逻辑（帧头解析、时长估算、进度状态机、时间格式化）由单元测试覆盖，见 [test/README.md](../test/README.md)。

## 启动时序

启用后系统自动创建 `liot_mp3_demo_thread`，串口 115200 可观察到以下启动序列：

```text
[demo_mp3_stream] demo start
[demo_mp3_stream] === MP3 DEMO START ===
[demo_mp3_stream] === MP3 DEMO START 2 ===
[demo_mp3_stream] === MP3 DEMO START 3 ===
```

随后进入 `lvgl_init()` → `demo_mp3_page_enter_async()`，等待约 8 秒（搜网）后自动开始播放。

真机实测启动片段（2026-10-10），可见预初始化早于触摸初始化，加载完成后才 `StreamStart`：

```text
[audio] MP3 preinit before touch begin
[demo_mp3_stream] AudioInit ret=0
[TP] init ok
[demo_mp3_stream] enter begin
[demo_mp3_stream] STATE STOPPED -> LOADING (demo_mp3_page_enter)
[demo_mp3_stream] load done ok=1
[demo_mp3_stream] StreamStart ret=0
```

## 硬件要求

- 项目：`L_CT4IT02_1698W`
- 屏幕：360x360 圆屏，复用工程 LVGL 显示和触摸配置
- 音频 Codec：ES8375，I2S slave 模式
- 外部 Flash：P25Q64（SPI1），用于存储 MP3 和封面
- 电源：AON 域供电 + LDO 3.3V（GPIO25/GPIO27 控制）

## 已知限制

- `Liot_AudioPlayMp3` 被 `AudioStop()` 强制中断后解码器内部状态不完整重置，导致切歌后下一首可能提前结束。当前版本只保留单首歌规避此问题。
- 冷启动后首次播放可能无声，通过 autokick（快速 pause/resume）机制解决。
- Makefile 不跟踪 `.incbin` 引用的文件变化，更换 MP3 后需手动删除 `demo_mp3_resources.o` 再编译。
