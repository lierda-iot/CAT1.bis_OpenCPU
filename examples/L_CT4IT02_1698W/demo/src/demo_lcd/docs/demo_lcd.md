# L_CT4IT02_1698W LCD 示例说明

更新时间：2026-10-10

## 1. 示例文件

- 源文件：`examples/L_CT4IT02_1698W/demo/src/demo_lcd/demo_lcd.c`
- 入口函数：`liot_lcd_demo_thread(void *argv)`

## 2. 示例用途

验证圆屏 LCD + 触摸链路：

- QSPI 初始化（GC9B72，LSPI2/CS0）
- PWM 背光亮度渐变（100→20）
- 画线 / 画圆 / 画矩形 / 显示 Lierda logo
- 触摸初始化（BL6178）与触摸画点
- AON / 3.3V 与 LDO（GPIO25/27）控制

## 3. 当前固定配置

- LCD：GC9B72，QSPI，LSPI PORT2，CS0，51MHz
- 背光：PWM0，pin100；复位：pin78
- 触摸：I2C1（pin66/pin57 复用），INT 使用 GPIO21（pin6）
- 启动延时：5s（等电源稳定）

## 4. 电源与引脚

- LDO 使能：`L_GPIO_25`、`L_GPIO_27`
- 背光 PWM：pin100；LCD 复位：pin78
- 触摸 I2C：pin66（Func2）、pin57（Func3）
- 触摸 INT：pin6 / GPIO21，下降沿

## 5. 编译开关

- 板级宏：`HWDEMO_LCD_EN=y`

## 6. 建议编译方式

```bash
make all PROJECT=L_CT4IT02_1698W MODEM=NT26F9D0 MODEMPKG=F9D_A BUILD_MODE=demo HWDEMO_LCD_EN=y
```

## 7. 运行方式

启用后系统启动自动创建任务。串口 115200。

## 8. 预期结果

### 8.1 预期关键日志

- `==== lcd demo start ====`
- `[TP] init ok`
- `[TP] INT enabled, waiting for touch...`
- 触摸后持续打印触摸点并绘制红点

上电握手阶段还会依次出现（顺序固定，内容随固件版本变化）：

- `ctp_bl_ts_init:Read chipID success:chipID = 23`
- `bl_auto_update_fw:fwLen = 7b8c` → `bl_update_fw_for_self_ctp:ret=0` → `bl_auto_update_fw: bl_update_fw success`
- 可能先打印 `BL6178 invalid chip_id: 0x0 (expect 0x23)`，随后仍以 `BL6178 init ok` 继续；本次真机触摸正常

### 8.2 预期行为

- 屏幕先显示十字线 → Lierda logo → 圆/矩形图形 → 白屏
- 背光从 100 逐步降到 20
- 触摸时在屏幕画出蓝色轨迹 + 红色当前点

### 8.3 验证状态

- 编译已验证（demo 组合）。
- 真机（2026-10-09）验证：初始化日志与上述顺序一致，随后持续输出 `[TP] touch0: evt=.. x=.. y=..` 与 `[TP] gesture: 1`，触摸轨迹随手指移动。

## 9. 当前边界

- 仅单屏 QSPI 通路；LSPI 双屏/其他 LCD 型号未覆盖。
- 驱动 `ctp_bl_ts_init()` 会在初始化时自动执行 BL6178 固件校验/升级（见 8.1）；应用层 `liot_tp_update_firmware()` 接口已预留但未调用。
