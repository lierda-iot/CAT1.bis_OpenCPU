# 工资计算器 Demo（L_CT4IT02_1698W）

基于 `lvgl_demo` 的 LVGL v8.4.0 横屏显示和触摸方案，实现工资实时计算、金币动画和 ES8375 金币音效演示。

## 功能

- 设置月薪，支持数字键盘输入，以及每次 1000 元的减/加按钮
- 设置金币音效触发间隔，支持数字键盘输入，以及每次 1 元的减/加按钮，默认 1 元
- 设置上班时间和下班时间，每月固定按 22 个工作日计算
- 显示 RTC 当前时间，并支持校准年、月、日、时、分
- 进入赚钱页后立即显示当天已赚金额，之后每秒重新计算并刷新
- 每跨过用户设置的新金额档位播放一次 `coin_burst.wav`
- 金币持续掉落；达到新音效档位时额外触发 10 个金币错峰、不同速度掉落
- 赚钱页可点击 `< Settings` 返回设置页修改参数
- 月薪、音效间隔和上下班时间保存到 NVS

## 设计背景

本 Demo 用于 L_CT4IT02_1698W 圆屏样机展示工资实时增长效果，同时验证以下功能链路：

- LVGL 横屏 UI 与触摸交互
- RTC 时间显示与设置
- NVS 参数保存与恢复
- ES8375 I2S 播放金币音效
- 360x360 圆屏上的金币动画表现

当前音频链路在冷启动后需要预热，程序启动后会后台播放数次金币音，用于唤醒 ES8375 播放链路。后续可替换为静音 PCM 预热，避免用户听到预热音。

## 构建与烧录

### 构建命令

```powershell
./build.bat build PROJECT=L_CT4IT02_1698W BUILD_MODE=demo MODEM=NT26F9D0 MODEMPKG=F9D_A HWDEMO_SOUND_ES8375_EN=n HWDEMO_SALARY_CALCULATOR_EN=y
```

### 烧录文件

```text
gccout/L_CT4IT02_1698W/L_CT4IT02_1698W_NT26F9D0_01.binpkg
```

## 资源文件

资源文件位于本目录：

- `coin_burst.wav`：金币音效，PCM WAV，16 kHz，16 bit，mono
- `coin_burst_wav.c`：由 `coin_burst.wav` 转换生成的内置音频数组
- `money_background.png`：用户提供的原始背景图，保留不修改
- `money_background_360x360.rgb565`：360x360 RGB565 原始图像资源

建议正式量产时将音效和背景图放到外部 extflash，例如：

- `/extflash/salary/coin_burst.wav`
- `/extflash/salary/money_background_360x360.rgb565`

当前代码已内置 `coin_burst_wav.c`，即使 extflash 中没有音效文件，也可以播放内置音效。

## 操作与预期

1. 烧录后进入工资计算器设置页。
2. 设置页顶部显示当前 RTC 时间；下方 RTC 设置滚轮会同步年月日时分。
3. 设置月薪、音效间隔、上班时间和下班时间。
4. 点击 `Confirm` 进入赚钱页，立即显示当天已经赚到的金额。
5. 金额每秒刷新；跨过设置的金额档位时播放金币音效，并触发 10 个金币错峰掉落。
6. 点击 `< Settings` 返回设置页，可继续修改参数。

## 硬件要求

- 项目：`L_CT4IT02_1698W`
- 屏幕：360x360 圆屏，复用工程 LVGL 显示和触摸配置
- 音频 Codec：ES8375，I2S slave 模式
- RTC：设备 RTC，用于工资实时计算
- NVS：保存工资参数

## 注意

- 本 Demo 面向 `L_CT4IT02_1698W` 工程，不是通用 demo。
- RTC 显示按 UTC+8 处理；设置时间时界面填写本地时间。
- 音效触发按“跨过金额档位”判断，不按每秒固定播放。
- 若冷启动后第一次无声，需关注 ES8375 上电、MCLK/I2S、PA/模拟通路稳定时序；当前版本用后台预热规避。
