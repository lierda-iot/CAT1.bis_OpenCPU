# L_CT4IT02_1698W SC7A20H G-Sensor 示例说明

更新时间：2026-10-10（日志优化后版本）

## 1. 示例文件

- 源文件：`examples/L_CT4IT02_1698W/demo/src/demo_sc7a20h/demo_sc7a20h.c`
- 核心逻辑：`examples/L_CT4IT02_1698W/demo/src/demo_sc7a20h/demo_sc7a20h_core.h`
- 入口函数：`liot_sc7a20h_demo_thread(void *argv)`

## 2. 示例用途

验证 SC7A20H 三轴加速度传感器 + LVGL 可视化链路：

- I2C1 总线与 pinmux 初始化
- 驱动初始化与参数配置（4G / 400Hz / 高性能 / 高通滤波 / FIFO）
- 摇晃检测（阈值 500mg、20ms，INT2）
- 双击检测（INT1）
- LVGL 显示加速度点与 X/Y/Z 数值
- 中断唤醒初始化

## 3. 当前固定配置

- 量程：`SC7A20H_RANGE_4G`
- ODR：`SC7A20H_ODR_400HZ`，模式：`HI_PERF`
- FIFO：enable，watermark 16
- Shake：INT2，阈值按事件级别（默认 500mg 级），时长 20ms
- Click：INT1，双击模式
- 启动延时：5s；I2C1 标准模式

## 4. 电源与引脚

- LDO 使能：`L_GPIO_25`、`L_GPIO_27`
- I2C1：SDA pin38（`SC7A20H_I2C_SDA_PIN`）、SCL pin39（`SC7A20H_I2C_SCL_PIN`）
- 中断：G_INT1_WKUP（点击）、G_INT2_WKUP（摇晃），上升沿唤醒

## 5. 编译开关

- 板级宏：`HWDEMO_SC7A20H_EN=y`
- 开启后自动引入 LVGL 与 GSENSOR 驱动源码

## 6. 建议编译方式

```bash
make all PROJECT=L_CT4IT02_1698W MODEM=NT26F9D0 MODEMPKG=F9D_A BUILD_MODE=demo HWDEMO_SC7A20H_EN=y
```

> 本次真机验证即使用该编译方式。

## 7. 运行方式

启用后系统启动自动创建任务。串口 115200；摇晃/敲击样机观察 LVGL 红点与数值变化。

采集时长约 13.5s（1347 条加速度样本，约 10ms/条）。静止时 X/Y/Z 在基线附近
（本次静止基线 Z≈510mg、X≈±8mg）；水平左右翻动时 X 出现约 -400mg 的明显偏移；
倾斜时 Z 相应减小。加速度输出为 12bit LSB，正值即 mg。

## 8. 预期结果

### 8.1 预期关键日志

- 初始化完成：`[SC7A20H] init complete`
- 周期加速度（约 10ms 一条）：`[SC7A20H] X=.. Y=.. Z=..`
- 摇晃中断：`[SC7A20H] shake: high=.. low=.. raw=0x..`
- 点击中断：`[SC7A20H] click: ../.. raw=0x..`
- 失败路径：`[SC7A20H] gpio config fail` / `[SC7A20H] i2c init fail` / `[SC7A20H] init fail`

本次真机日志实际输出（截取）：

```
[SC7A20H] ODR set: 0x07
[SC7A20H] click config: OK
[SC7A20H] shake config: OK
[SC7A20H] config apply: OK
[SC7A20H] init complete
[SC7A20H] X=69 Y=37 Z=523
[SC7A20H] X=63 Y=34 Z=527
[SC7A20H] X=69 Y=38 Z=521
...
```

初始化阶段仅 5 行配置日志，之后为连续的加速度数据；不再有寄存器级
`Read1/Write2 Reg` 刷屏。

> 寄存器级 I2C 读写默认不打印；需要排障时把
> `examples/L_CT4IT02_1698W/driver/src/sc7a20h/HAL/sc7a20h_hal_cfg.c`
> 中的 `SC7A20H_I2C_TRACE_ENABLE` 置 1。

### 8.2 预期行为

- 屏幕中央红点随姿态移动，底部显示 `X:... Y:... Z:...`
- 摇晃超过阈值计数触发事件，双击触发点击事件

### 8.3 验证状态

- 真机（2026-10-09）验证：上电配置只剩 5 行（`ODR set` / `click config` / `shake config` / `config apply` / `init complete`），之后为纯 `X/Y/Z` 数据且随翻动/倾斜实时变化；关闭 `SC7A20H_I2C_TRACE_ENABLE` 后不再出现寄存器级刷屏。
- 编译已验证（demo 组合）。
- 纯逻辑（摇晃/点击判定阈值）由单元测试覆盖，见 [test/README.md](../test/README.md)。
- 真机表现以实际串口日志 + 屏幕表现为准。

## 9. 当前边界

- 未包含手势/计步等其他 SC7A20H 功能。
- LVGL 展示依赖 LCD 通路正常。
