# L_CT4IT02_1698W Raw ExtFlash 示例说明

更新时间：2026-07-20

## 1. 示例文件

- 源文件：`examples/L_CT4IT02_1698W/demo/src/demo_extflash_p25q64/demo_extflash_p25q64.c`
- 入口函数：`liot_extflash_p25q64_demo_thread(void *argv)`

## 2. 示例用途

该示例用于验证板载 extflash 的基础 raw 访问链路，包含：

- extflash 上电
- SPI 引脚初始化
- extflash 驱动初始化
- 4KB 擦除
- 256B 写入 / 读取 / 校验
- 4KB 扇区读改写测试

## 3. 当前固定配置

根据当前源码，示例固定使用以下配置：

- Flash 名称：`P25Q64_8MB`
- SPI 端口：`1`
- Flash 基地址：`0x000000`
- Flash 总容量：`0x00800000`
- 擦除块大小：`4096`
- 基础测试偏移：`0x000000`
- 基础测试长度：`256`
- RMW 测试偏移：`0x001000`

## 4. 电源与引脚

### 4.1 电源控制

当前源码使用：

- `L_GPIO_25`
- `L_GPIO_27`

当前处理顺序为：

1. 先直接执行 `Liot_GpioInit(..., L_IO_HIGH, ...)`
2. 如果失败，再回退到 `Liot_SetPinFunc(..., L_PIN_FUNC_0) + Liot_GpioInit(...)`

### 4.2 SPI 引脚

当前源码固定使用：

- MOSI pad `63`
- MISO pad `62`
- SCLK pad `49`
- CS pad `64`
- CS gpio `L_GPIO_12`

## 5. 编译开关

板级宏：

- `HWDEMO_EXTFLASH_P25Q64_EN`

默认在 `examples/L_CT4IT02_1698W/config` 中为关闭状态。

## 6. 建议编译方式

当前板级使用 `NT26F9D0`，建议显式带上 `F9D_A`，并临时覆盖 `CODE_REGION_ADJUST=0`：

```bash
make all PROJECT=L_CT4IT02_1698W MODEM=NT26F9D0 MODEMPKG=F9D_A \
    CODE_REGION_ADJUST=0 \
    HWDEMO_EXTFLASH_P25Q64_EN=y \
    HWDEMO_EXTFLASH_FS_P25Q64_EN=n
```

## 7. 运行方式

启用 `HWDEMO_EXTFLASH_P25Q64_EN` 后，系统启动会自动创建该任务，无需手工调用。

串口波特率：

```text
115200
```

## 8. 当前预期结果

### 8.1 预期日志前缀

```text
[p25q64_raw]
```

### 8.2 预期关键日志

当前应重点关注以下日志：

- `demo start`
- `LDO33 enable enabled via direct gpio=25` 或 fallback 成功日志
- `VCC3V3 enable enabled via direct gpio=27` 或 fallback 成功日志
- `flash init ok`
- `raw_basic pass`
- `raw_rmw pass`
- `demo done`

### 8.3 预期行为

当前预期该示例能够完成：

- extflash 初始化成功
- 4KB 擦除成功
- 256B 写入 / 读取 / 校验成功
- 4KB 扇区 RMW 成功
- `liot_flash_deinit_ext()` 正常返回

## 9. 当前边界

该示例当前只覆盖 raw 基础访问，不包含以下内容：

- RDID / MDID 识别
- 多 profile 兼容探测
- 跨页 / 跨扇区 / 尾地址完整覆盖
- 多轮循环稳定性测试
- LittleFS 文件系统验证
