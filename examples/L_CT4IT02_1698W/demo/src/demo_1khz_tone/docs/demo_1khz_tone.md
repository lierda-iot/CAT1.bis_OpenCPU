# L_CT4IT02_1698W 1kHz Tone 示例说明

更新时间：2026-10-10

## 1. 示例文件

- 源文件：`examples/L_CT4IT02_1698W/demo/src/demo_1khz_tone/demo_1khz_tone.c`
- 核心逻辑：`examples/L_CT4IT02_1698W/demo/src/demo_1khz_tone/demo_tone_core.h`
- 入口函数：`liot_sound_es8375_demo_thread(void *argv)`

## 2. 示例用途

通过板载 ES8375 播放 `.incbin` 内嵌的 1 kHz 正弦 PCM 音频，用于验证音频链路：

- AON / 3.3V 供电与 LDO（GPIO25/27）控制
- ES8375 I2S 初始化与播放
- PCM 内嵌资源循环播放
- 音量 0–100 控制（AT 命令 `AT+TONEVOL`，见第 10 节）

## 3. 当前固定配置

- 音频源：`demo/src/demo_mp3/sine_1khz_16k_mono_s16le.pcm`（内嵌，循环播放）
- 采样率：16 kHz，16bit，单声道
- Codec：ES8375，I2S slave，默认音量 100
- 播放间隔：播放结束后短延时继续循环
- AT 音量：`AT+TONEVOL=<0-100>` / `AT+TONEVOL?` / `AT+TONEVOL=?`（范围 `(0-100)`）

## 4. 电源与引脚

- 电源：`L_GPIO_25`、`L_GPIO_27` 输出高使能 LDO
- Audio：I2C0 + I2S0（ES8375 板载通路）

## 5. 编译开关

- 板级宏：`HWDEMO_SOUND_ES8375_EN=y`（config 默认 n，需显式开启）
- 互斥关系：`HWDEMO_XIAOZHI_EN=n`、`HWDEMO_MP3_STANDALONE_EN=n` 时才会编译本 demo
- 说明：本 demo 同时编译 `demo_1khz_tone.c`（播放主体）与 `demo_1khz_tone_at.c`（AT 音量命令），两者同编同删，无独立开关。

## 6. 建议编译方式

```bash
make all PROJECT=L_CT4IT02_1698W MODEM=NT26F9D0 MODEMPKG=F9D_A BUILD_MODE=demo HWDEMO_SOUND_ES8375_EN=y HWDEMO_XIAOZHI_EN=n HWDEMO_MP3_STANDALONE_EN=n
```

## 7. 运行方式

启用后系统启动自动创建任务，无需手工调用。串口 115200。

## 8. 预期结果

### 8.1 预期日志前缀

```text
[tone-1khz]
```

### 8.2 预期关键日志

- `[tone-1khz] standalone demo start`
- `[tone-1khz] AT ready: AT+TONEVOL=<0-100>, AT+TONEVOL?`
- `[tone-1khz] audio event=...`（循环播放事件）
- `AT+TONEVOL=50` 设置后返回 `+TONEVOL: 50`

### 8.3 AT 音量命令

- `AT+TONEVOL=<0-100>`：设置音量，成功返回 `+TONEVOL: <vol>`；非法值返回 `ERROR`
- `AT+TONEVOL?`：查询当前音量，返回 `+TONEVOL: <vol>`
- `AT+TONEVOL=?`：查询范围，返回 `+TONEVOL: (0-100)`

命令处理器内部调用 `demo_tone_set_volume()` 同步设置软/Codec 音量；启动后串口即可使用。

### 8.4 验证状态

- 编译已验证（demo 模式组合编译通过）。
- 真机（2026-10-10）验证：`AudioInit ret=0 len=32000` → `volume=100 sw ret=0 codec ret=0`，随后持续 `audio event=0/2` 与 `loop=N play ret=0` 循环播放；`AT+TONEVOL=50` 返回 `+TONEVOL: 50` + `OK`，`AT+TONEVOL?` 返回 `+TONEVOL: 100`（默认值）。
- 纯逻辑（音量范围/set-get）已由单元测试覆盖，见 [test/README.md](../test/README.md)。
- 音频以串口日志 + 喇叭实听为准。

## 9. 当前边界

- 仅固定 1 kHz 正弦音，不包含多音频源切换。
- 不包含 UI；如需交互请用 demo_xiaozhi / demo_mp3 等 LVGL 方案。
- AT 音量命令仅服务 tone demo；xiaozhi/mp3_standalone 使用各自的音量命令（`AT+MP3VOL`）。

## 10. AT 音量命令说明（原 demo_1khz_tone_at）

### 10.1 示例文件

- 源文件：`examples/L_CT4IT02_1698W/demo/src/demo_1khz_tone/demo_1khz_tone_at.c`
- 注册入口：`demo_tone_atcmd_init()`（由 tone demo 任务启动时调用）

### 10.2 示例用途

为 1 kHz tone demo 提供 AT 音量控制命令，验证 AT 命令注册与动态调节链路。

### 10.3 支持的 AT 命令

- `AT+TONEVOL=<0-100>`：设置音量，成功返回 `+TONEVOL: <vol>`
- `AT+TONEVOL?`：查询当前音量，返回 `+TONEVOL: <vol>`
- `AT+TONEVOL=?`：查询范围，返回 `+TONEVOL: (0-100)`

### 10.4 预期结果

- 设置：`AT+TONEVOL=30` → `+TONEVOL: 30`
- 查询：`AT+TONEVOL?` → `+TONEVOL: 30`
- 非法值：`AT+TONEVOL=101` → `ERROR`

### 10.5 当前边界

- 仅服务 tone demo；xiaozhi/mp3_standalone 使用各自的音量命令（`AT+MP3VOL`）。
