# L_CT4IT02_1698W MP3 Standalone 示例说明

更新时间：2026-10-09

## 1. 示例文件

- 源文件：`examples/L_CT4IT02_1698W/demo/src/demo_mp3_standalone/demo_mp3_standalone.c`
- 核心逻辑：`examples/L_CT4IT02_1698W/demo/src/demo_mp3_standalone/demo_mp3_standalone_core.h`
- 入口函数：`liot_sound_es8375_demo_thread(void *argv)`

## 2. 示例用途

无 UI 的最小 MP3 播放验证：

- 内嵌 `.mp3` 资源（跳过 44 字节 ID3 头）循环播放
- ES8375 I2S 初始化（44.1kHz 通路）
- 音量 0–100 控制，AT 命令 `AT+MP3VOL`

## 3. 当前固定配置

- 音频源：`demo/src/demo_mp3/because of you_-12dB.mp3`（内嵌，循环）
- 采样率：44.1 kHz（`L_AUD_44K_SAMPLES`）
- Codec：ES8375，I2S slave，默认音量 100

## 4. 电源与引脚

- 电源：`L_GPIO_25`、`L_GPIO_27` 输出高使能 LDO
- Audio：I2C0 + I2S0（ES8375 板载通路）

## 5. 编译开关

- 板级宏：`HWDEMO_SOUND_ES8375_EN=y`（config 默认 n，需显式开启）
- 独占开关：`HWDEMO_MP3_STANDALONE_EN=y`（开启后不编译 tone / xiaozhi）

## 6. 建议编译方式

```bash
make all PROJECT=L_CT4IT02_1698W MODEM=NT26F9D0 MODEMPKG=F9D_A BUILD_MODE=demo HWDEMO_SOUND_ES8375_EN=y HWDEMO_MP3_STANDALONE_EN=y
```

## 7. 运行方式

启用后系统启动自动创建任务。串口 115200。

AT 音量：

- `AT+MP3VOL=50` → `+MP3VOL: 50`
- `AT+MP3VOL?` → `+MP3VOL: 50`

## 8. 预期结果

### 8.1 预期日志前缀

```text
[mp3-standalone]
```

### 8.2 预期关键日志

- `[mp3-standalone] start size=... offset=44`
- `[mp3-standalone] AudioInit ret=0`
- `[mp3-standalone] play ret=0 len=...`
- `[mp3-standalone] playback finished wait_ret=0; loop`

### 8.3 验证状态

- 编译已验证（demo 组合）。
- 纯逻辑（音量映射）由单元测试覆盖，见 [test/README.md](../test/README.md)。
- 真机播放效果以实际串口日志 + 喇叭声音为准。

## 9. 当前边界

- 无 UI、无暂停/快进控制。
- 仅内嵌单曲循环；外部文件播放请用 demo_mp3（LVGL）。
