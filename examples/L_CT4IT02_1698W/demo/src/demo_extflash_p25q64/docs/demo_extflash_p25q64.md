# L_CT4IT02_1698W Raw ExtFlash 示例说明

更新时间：2026-10-09

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

当前板级使用 `NT26F9D0`，建议显式带上 `F9D_A`。`CODE_REGION_ADJUST` 已由 `examples/L_CT4IT02_1698W/config` 统一设置，无需在命令中覆盖：

```bash
make all PROJECT=L_CT4IT02_1698W MODEM=NT26F9D0 MODEMPKG=F9D_A BUILD_MODE=demo HWDEMO_EXTFLASH_P25Q64_EN=y HWDEMO_EXTFLASH_P25Q64_LFS_EN=n HWDEMO_LVGL_PHOTO_LFS_EN=n
```

说明：

- `BUILD_MODE=demo` 显式声明 demo 构建模式。
- 建议同时显式关闭 `HWDEMO_EXTFLASH_P25Q64_LFS_EN` / `HWDEMO_LVGL_PHOTO_LFS_EN`，避免多个 extflash 使用者同时拉起；应用层已由 `demo_extflash_p25q64_mgr`（引用计数）统一管理并发复用，观察单 demo 时仍建议只开目标功能。
- `HWDEMO_SOUND_ES8375_EN` 默认 n（声音三选一默认关闭），无需显式设置。

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

### 8.3 实测日志（真机 2026-10-08，生命周期引用计数修复后）

```text
[p25q64_raw] demo start board=L_CT4IT02_1698W flash=P25Q64_8MB total=0x00800000
[extflash_mgr] LDO33 enable enabled via direct gpio=25
[extflash_mgr] VCC3V3 enable enabled via direct gpio=27
[extflash_mgr] owner=p25q64_raw flash ready refcount=1 spi=1 base=0x000000 total=0x00800000
[p25q64_raw] flash init ok spi=1 base=0x000000 total=0x00800000
[p25q64_raw] raw_basic pass offset=0x000000 size=256 erase_tick=21 write_tick=3 read_tick=0 verify_tick=0 tail=0x30
[p25q64_raw] raw_rmw pass offset=0x001000 size=4096 read_tick=0 erase_tick=40 write_tick=96 verify_tick=0 data=AA BB CC DD total_tick=161
[extflash_mgr] owner=p25q64_raw last owner deinit, refcount=0
[p25q64_raw] demo done
```

说明：

- 实测确认 256B 写入/读取/校验与 4KB 扇区读改写均通过，`demo done` 正常收尾，未再出现复位。
- 修复前问题表现：`spiFlashInit( port = 1 ) failed : -1` / `Failed to create mutex for gSpiMutex` 后 watchdog 复位，反映应用层并发复用同一 extflash 资源缺少统一生命周期管理的问题。

### 8.4 预期行为

当前预期该示例能够完成：

- extflash 初始化成功
- 4KB 擦除成功
- 256B 写入 / 读取 / 校验成功
- 4KB 扇区 RMW 成功
- `liot_flash_deinit_ext()` 正常返回

### 8.5 生命周期说明

`demo_extflash_p25q64` / `demo_extflash_p25q64_lfs` / `demo_lvgl_photo_lfs` 通过 `demo_extflash_p25q64_mgr`（引用计数）统一管理 extflash 电源、SPI 与驱动生命周期：

- 首个使用者：初始化硬件并设置 `refcount=1`
- 后续使用者：仅递增 `refcount`
- 最后一个使用者释放：递减至 `refcount=0` 并执行 `liot_flash_deinit_ext()`
- 日志关键字：`owner=... flash ready refcount=N` / `last owner deinit, refcount=0`

## 9. 当前边界

该示例当前只覆盖 raw 基础访问，不包含以下内容：

- RDID / MDID 识别
- 多 profile 兼容探测
- 跨页 / 跨扇区 / 尾地址完整覆盖
- 多轮循环稳定性测试
- LittleFS 文件系统验证
