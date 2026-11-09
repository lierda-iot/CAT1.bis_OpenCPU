# EC718 平台限制与固定约束

范围：`examples/L_CT4IT02_1698W`。标准构建命令、basePkg 门禁、AI 文档与 git 提交规范
以仓库根 `AGENTS.md` 为准，本文件不再重复。

## 1. 通用编译说明

当前模组 `NT26F9D0` + `MODEMPKG=F9D_A`，`BUILD_MODE` 二选一：

```bash
# 应用模式：默认 APP_BAJI_EN=y，可用 APP_*_EN=y 叠加其他应用
make all PROJECT=L_CT4IT02_1698W MODEM=NT26F9D0 MODEMPKG=F9D_A BUILD_MODE=app

# demo 模式
make all PROJECT=L_CT4IT02_1698W MODEM=NT26F9D0 MODEMPKG=F9D_A BUILD_MODE=demo
```

`LIOT_LVGL_UI_WATCH_PROFILE`、`GCC_PATH`、`rtk` 包装和 basePkg 门禁见仓库根 `AGENTS.md` §15。

## 2. 不要依赖 64 位格式化输出

关键业务路径不要依赖 `%lld`、`%llu`、`PRI64`、`PRIu64`、`SCNu64` 等 64 位格式化输出；
时间戳、尺寸、索引和计数优先使用 32 位变量。

## 3. `liot_trace` 单条日志最多 4 个动态参数

字段更多时拆成多条，或先输出摘要再输出分组细节；不要依赖历史上偶尔可通过的 8 参数、
9 参数边界行为。

## 4. 十六进制预览使用保守写法

- 优先用 `0x%x` 打印单个字段。
- 避免批量 `"%02X %02X ..."` 格式；必须打印多字节时拆成多条短日志。

```c
/* 推荐 */
liot_trace("buf_len=%u\n", len);
liot_trace("first4=0x%x 0x%x 0x%x 0x%x\n", b0, b1, b2, b3);

/* 不推荐 */
liot_trace("first8=%02X %02X %02X %02X %02X %02X %02X %02X\n", ...);
```

## 5. 调试日志换行

`LIOT_PRINTF`（[lierda_log.h](driver/inc/lierda_log.h)）已自带 `\r\n`，格式串不要重复
写换行；直接调用 `liot_trace` 时在格式串末尾补 `\n`，避免多条日志粘连。
