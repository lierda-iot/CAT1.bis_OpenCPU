# L_CT4IT02_1698W GNSS（CC1161W）示例说明

更新时间：2026-10-09

## 1. 示例文件

- 源文件：`examples/L_CT4IT02_1698W/demo/src/demo_gnss/demo_gnss.c`
- 核心逻辑：`examples/L_CT4IT02_1698W/demo/src/demo_gnss/demo_gnss_core.h`
- 入口函数：`liot_gnss_demo_thread(void *argv)`

## 2. 示例用途

验证 CC1161W GNSS 模组 UART/NMEA 链路：

- AON / 3.3V 供电与模组电源（GPIO25/27）控制
- UART2 115200 8N1 初始化与 pinmux
- NMEA 逐行解析：坐标转 e7、GGA/GSV 统计、无效定位告警
- 原始字节采样（静默时输出 hex dump）
- AGNSS：`demo_gnss_query_assistance()` 查询 `$AIDINFO`，`demo_gnss_inject_assistance()` 注入辅助数据

## 3. 当前固定配置

- UART：`L_UART2`，115200 8N1
- 接收：pin55（GPIO6/Func2）← GNSS_TX
- 发送：pin56（GPIO7/Func2）→ GNSS_RX
- 电源：GPIO25/27 输出高，稳定 500ms
- 统计日志周期：10s

## 4. 电源与引脚

- 模组供电使能：`L_GPIO_25`、`L_GPIO_27`
- UART pinmux：RX pin55 / TX pin56，均为 Func2

## 5. 编译开关

- 板级宏：`HWDEMO_GNSS_EN=y`
- demo 模式与 app 模式均可用（app 模式下由 `APP_MAP_EN=y` 自动开启）

## 6. 建议编译方式

demo 模式单独验证：

```bash
make all PROJECT=L_CT4IT02_1698W MODEM=NT26F9D0 MODEMPKG=F9D_A BUILD_MODE=demo HWDEMO_GNSS_EN=y
```

app 模式（随地图功能）直接使用 `APP_MAP_EN=y` 的常规 app 编译。

## 7. 运行方式

启用后系统启动自动创建任务。串口 115200；GNSS 模组天线朝上放置，室外或窗边等待定位。

## 8. 预期结果

### 8.1 预期日志前缀

```text
[gnss-cc1161w]
```

### 8.2 预期关键日志

- `start UART2 115200 8N1`
- `UART ready, enable peripheral power`
- 每 10s：`callbacks= bytes= NMEA lines=`
- 有效定位：`GGA ... fix=1 ... sats=...`
- 首定位：`AGNSS FIRST FIX TTFF=...`（注入辅助后）
- 静默时：`raw sample hex` + `RX pin55 level=`

### 8.3 实测日志（真机历史记录）

```text
[gnss-cc1161w] GGA utc= lat=0000.00000N lon=00000.00000E fix=0 sats=00 hdop=0.000 alt=0.0
```

说明：

- 真机记录仅到「UART/NMEA 链路正常、无定位 fix=0」的解析输出；有效定位（`fix=1`、`sats>0`、`AGNSS FIRST FIX`）日志待室外复测后补充。
- 因此当前验证状态按「编译已验证 + 链路级日志确认」，不标称有效定位已验证。

### 8.4 验证状态

- 编译已验证（demo 组合）。
- 历史真机日志可见 GGA 解析输出（无定位），详见 8.3。
- 纯逻辑（坐标 e7 换算、NMEA 字段拆分）由单元测试覆盖，见 [test/README.md](../test/README.md)。

## 9. 当前边界

- 仅原始 NMEA/AGNSS 链路，不含地图定位展示（地图在 app 模式）。
- 需要室外/窗边环境才能获得有效定位。
