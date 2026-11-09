# L_CT4IT02_1698W ES8375 板载音频通路说明

更新时间：2026-10-09

## 1. 概述

`L_CT4IT02_1698W` 板载 `ES8375` 音频通路，总开关为 `HWDEMO_SOUND_ES8375_EN`。实际运行实现由 demo 模式的「声音三选一」提供，`demo_sound_es8375.c` 保留为参考实现，当前未挂接 Makefile（不参与编译）。

当前配置：

- 码率芯片：`ES8375`
- I2C：`i2c0`
- SDA：GPIO38
- SCL：GPIO39
- I2S：`i2s0`
- 外置 PA：无，`paGpioNum = -1`
- 电源使能：`L_GPIO_25`、`L_GPIO_27` 输出高

## 2. 前置条件

`config/iodriver.ini` 需保持 `i2c0` 映射：

```ini
i2c0_default=0
i2c0_pin_scl=39,2
i2c0_pin_sda=38,2
```

## 3. 编译开关与实现映射

`HWDEMO_SOUND_ES8375_EN` 是总开关（config 默认 `n`，需显式 `=y` 开启）。开启后按以下优先级三选一：

| 条件 | 编译实现 | 日志前缀 | 入口 |
| --- | --- | --- | --- |
| `HWDEMO_MP3_STANDALONE_EN=y` | `demo_mp3_standalone`（独占） | `[mp3-standalone]` | `liot_sound_es8375_demo_thread` |
| 否则 `HWDEMO_XIAOZHI_EN=y`（需显式开启，config 默认 `n`） | `demo_xiaozhi`（独占） | `[xiaozhi]` | `liot_sound_es8375_demo_thread`（内部创建 `xiaozhi_main`） |
| 否则 | `demo_1khz_tone`（含 AT 音量命令） | `[tone-1khz]` | `liot_sound_es8375_demo_thread` |

说明：

- `HWDEMO_SOUND_ES8375_EN` 同时控制 `0demo_main.c` 创建 `liot_sound_es8375_demo_thread`。
- `demo_mp3_standalone` / `demo_xiaozhi` / `demo_1khz_tone` 三个实现共享同一个入口符号，互斥编译。
- 声音 demo 默认关闭（config 默认 `n`）；需要时显式传 `HWDEMO_SOUND_ES8375_EN=y` 开启。

## 4. 各实现建议编译方式

### 4.1 MP3 Standalone（无 UI 循环播放）

```bash
make all PROJECT=L_CT4IT02_1698W MODEM=NT26F9D0 MODEMPKG=F9D_A BUILD_MODE=demo HWDEMO_SOUND_ES8375_EN=y HWDEMO_MP3_STANDALONE_EN=y
```

详见 [demo_mp3_standalone.md](../demo_mp3_standalone/docs/demo_mp3_standalone.md)。

### 4.2 XiaoZhi AI 语音

```bash
make all PROJECT=L_CT4IT02_1698W MODEM=NT26F9D0 MODEMPKG=F9D_A BUILD_MODE=demo HWDEMO_SOUND_ES8375_EN=y HWDEMO_XIAOZHI_EN=y HWDEMO_MP3_STANDALONE_EN=n
```

详见 [demo_xiaozhi.md](../demo_xiaozhi/docs/demo_xiaozhi.md)。

### 4.3 1kHz Tone（最小音频链路）

```bash
make all PROJECT=L_CT4IT02_1698W MODEM=NT26F9D0 MODEMPKG=F9D_A BUILD_MODE=demo HWDEMO_SOUND_ES8375_EN=y HWDEMO_XIAOZHI_EN=n HWDEMO_MP3_STANDALONE_EN=n
```

详见 [demo_1khz_tone.md](../demo_1khz_tone/docs/demo_1khz_tone.md)（含 AT 音量命令章节）。

## 5. 与 MP3 / Salary 的关系

- `HWDEMO_MP3_EN=y`（LVGL MP3 播放器）走独立的 `liot_mp3_demo_thread`，与声音三选一互不依赖；`HWDEMO_SOUND_ES8375_EN` 默认 n，无需显式关闭。
- `HWDEMO_SALARY_CALCULATOR_EN=y` 会自动联动 `HWDEMO_MP3_EN=y`，金币音效同样走 ES8375；声音三选一默认关闭，无需显式关闭。

## 6. 参考实现说明

`demo/src/demo_sound_es8375/demo_sound_es8375.c` 保留在仓库中作为 ES8375 通路参考实现，当前未挂接 Makefile。文件内 `#define TEST_PLAY` / `#define TEST_RECORD` 仅用于手工单独编译时的录放回环测试。

## 7. 排障

- 若无音频响应，优先检查 `config/iodriver.ini` 与 GPIO38/GPIO39 连线。
- 检查 GPIO25/27 供电使能是否生效（demo 日志应有 LDO 使能输出）。
- 冷启动首次无声时，确认播放器 autokick / 预热逻辑已执行。
