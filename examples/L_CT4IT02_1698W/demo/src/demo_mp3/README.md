# MP3 播放器 Demo（L_CT4IT02_1698W）

基于 LVGL v8.4.0 横屏显示和触摸方案，实现 MP3 播放器界面、专辑封面显示、播放/暂停控制和进度条演示。

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

```powershell
./build.bat build PROJECT=L_CT4IT02_1698W BUILD_MODE=demo MODEM=NT26F9D0 MODEMPKG=F9D_A HWDEMO_SOUND_ES8375_EN=n HWDEMO_MP3_EN=y
```

### 烧录文件

```text
gccout/L_CT4IT02_1698W/L_CT4IT02_1698W_NT26F9D0_01.binpkg
```

## 资源文件

资源文件位于本目录：

- `because of you.mp3`：MP3 音源，CBR 56 kbps，44.1 kHz，mono，无 ID3 标签
- `because of you.jpg`：用户提供的原始封面图，保留不修改
- `because_of_you_cover_360x360.rgb565`：360x360 RGB565 原始图像资源
- `demo_mp3_resources.c`：通过 `.incbin` 将 MP3 和封面嵌入固件

首次烧录时 MP3 和封面会自动写入外部 SPI Flash 文件系统：

- `/flash/mp3/because_of_you.mp3`
- `/flash/mp3/because_of_you_cover_360x360.rgb565`

## 操作与预期

1. 烧录后开机，等待搜网完成（约 8 秒）。
2. 自动进入播放器页面，显示封面和歌曲信息。
3. 自动开始播放 Because Of You。
4. 触摸播放/暂停按钮可暂停和恢复播放。
5. 播放完成后自动停止，可手动再次播放。

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
