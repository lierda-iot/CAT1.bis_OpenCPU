# L_CT4IT02_1698W LittleFS ExtFlash 示例说明

更新时间：2026-07-20

## 1. 示例文件

- 源文件：`examples/L_CT4IT02_1698W/demo/src/demo_extflash_fs_p25q64/demo_extflash_fs_p25q64.c`
- 入口函数：`liot_extflash_fs_p25q64_demo_thread(void *argv)`

## 2. 示例用途

该示例用于验证板载 extflash 上的 LittleFS 基础链路，包含：

- extflash 上电
- SPI 引脚初始化
- extflash 驱动初始化
- LittleFS mount
- mount 失败后单次 format + remount
- 目录创建
- 640B 文件写入 / 读取 / 校验
- 目录遍历

## 3. 当前固定配置

根据当前源码，示例固定使用以下配置：

- Flash 名称：`P25Q64_8MB`
- SPI 端口：`1`
- Flash 基地址：`0x000000`
- Flash 总容量：`0x00800000`
- FS 基地址：`0x010000`
- FS 总大小：`0x200000`
- FS block size：`4096`
- FS read size：`256`
- FS prog size：`256`
- 测试目录：`/flash`
- 测试文件：`/flash/p25q64_basic.bin`
- 测试文件长度：`640`

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

- `HWDEMO_EXTFLASH_FS_P25Q64_EN`

默认在 `examples/L_CT4IT02_1698W/config` 中为关闭状态。

## 6. 建议编译方式

当前板级使用 `NT26F9D0`，建议显式带上 `F9D_A`，并临时覆盖 `CODE_REGION_ADJUST=0`：

```bash
make all PROJECT=L_CT4IT02_1698W MODEM=NT26F9D0 MODEMPKG=F9D_A \
    CODE_REGION_ADJUST=0 \
    HWDEMO_EXTFLASH_P25Q64_EN=n \
    HWDEMO_EXTFLASH_FS_P25Q64_EN=y
```

## 7. 运行方式

启用 `HWDEMO_EXTFLASH_FS_P25Q64_EN` 后，系统启动会自动创建该任务，无需手工调用。

串口波特率：

```text
115200
```

## 8. 当前预期结果

### 8.1 预期日志前缀

```text
[p25q64_fs]
```

### 8.2 预期关键日志

当前应重点关注以下日志：

- `demo start`
- `flash init ok`
- `fs mount ok`
- `file_basic pass`
- `summary`
- `demo done`

如果首次挂载失败，当前源码预期会先输出：

- `liot_finit_ext failed ... try format once`

随后再执行一次：

- `liot_fformat_ext`
- `liot_finit_ext`

### 8.3 预期行为

当前预期该示例能够完成：

- extflash 初始化成功
- LittleFS mount 成功
- 如果首次 mount 失败，可单次 format 后重新 mount
- `/flash` 目录存在或可创建
- 640B 文件写入 / 读取 / 校验成功
- 目录遍历成功
- `liot_fdeinit_ext()` 正常返回

## 9. 当前边界

该示例当前只覆盖 LittleFS 基础 smoke 流程，不包含以下内容：

- rename 测试
- remount 持久化验证
- 大文件 / 长时间循环稳定性测试
- 多 profile 兼容探测
- RDID / MDID 识别输出
