# ES8375 板载音频 Demo

## 概述

`demo_sound_es8375.c` 用于验证 `L_CT4IT02_1698W` 板载 `ES8375` 音频通路。

当前配置：

- 码率芯片：`ES8375`
- I2C：`i2c0`
- SDA：GPIO38
- SCL：GPIO39
- I2S：`i2s0`
- 外置 PA：无，`paGpioNum = -1`

## 前置条件

`config/iodriver.ini` 需保持 `i2c0` 映射：

```ini
i2c0_default=0
i2c0_pin_scl=39,2
i2c0_pin_sda=38,2
```

## 启用方式

在 `examples/L_CT4IT02_1698W/Makefile` 中开启：

```make
HWDEMO_SOUND_ES8375_EN ?= y
```

或命令行编译时临时开启：

```bash
make build PROJECT=L_CT4IT02_1698W HWDEMO_SOUND_ES8375_EN=y
```

## 编译结果

开启后会编译：

- `examples/L_CT4IT02_1698W/demo/src/demo_sound_es8375.c`
- 宏定义 `HWDEMO_SOUND_ES8375_EN`

`0demo_main.c` 会自动创建 `liot_sound_es8375_demo_thread`。

## 运行说明

当前默认是播放测试：

```c
#define TEST_PLAY
//#define TEST_RECORD
```

如需录放回环测试，可切换 `demo_sound_es8375.c` 中的宏。

## 备注

- 本板无外置 PA，`paGpioNum` 已设置为 `-1`。
- 若无音频响应，优先检查 `config/iodriver.ini` 和 GPIO38/GPIO39 连线。
