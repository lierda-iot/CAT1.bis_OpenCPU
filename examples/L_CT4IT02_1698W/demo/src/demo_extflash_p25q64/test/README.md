# demo_extflash_p25q64 / demo_extflash_p25q64_lfs 核心逻辑单元测试

## 作用

两个 P25Q64 外部 Flash demo（raw 读写与 LittleFS 文件系统版）共享三处纯逻辑：

- raw 版填充模式 `buffer[i] = seed + i`；
- lfs 版填充模式 `buffer[i] = seed + i*3`；
- tick 耗时计算（`uint32_t` 回绕减法）。

这些逻辑被抽到 header-only 的 [`demo_extflash_p25q64_core.h`](../demo_extflash_p25q64_core.h)，
生产 `.c` 中的同名 static 函数改为调用 core 内联实现，行为不变。测试在主机端直接验证，
不需要 RTOS tick、GPIO 或 Flash 硬件。

## 如何触发

单独运行：

```sh
make -C examples/L_CT4IT02_1698W/demo/src/demo_extflash_p25q64/test test
```

通过总入口运行（模块名 `extflash_p25q64`）：

```sh
make -C examples/L_CT4IT02_1698W/tests host-test MODULE=extflash_p25q64
```

冒烟门（模块内单用例快捷校验）：

```sh
make -C examples/L_CT4IT02_1698W/tests host-test-smoke MODULE=extflash_p25q64
```

> 注：Unity 当前 `-n` 参数可解析但不跳过执行；只看单个用例请配合
> `grep '<用例名>.*PASS'` 使用。

查看用例清单 / 清理产物：

```sh
make -C examples/L_CT4IT02_1698W/demo/src/demo_extflash_p25q64/test list-tests
make -C examples/L_CT4IT02_1698W/demo/src/demo_extflash_p25q64/test clean
```

## 用例与断言

### test_fill_pattern_sequence_and_wrap

验证 raw 与 lfs 两种填充模式与回绕边界：raw `seed + i`（seed `0x31` 时
`buf[0]=0x31`、`buf[5]=0x36`、`buf[15]=0x40`；seed `0xFE` 验证 255→0 的
`uint8_t` 回绕）；lfs 步长 3（seed `0x41` 时 `buf[2]=0x47`、`buf[5]=0x50`；
seed `0xFE` 时 `buf[3]=0x07`）；`NULL` 缓冲调用不崩溃、不写内存（生产实现
新增的保护路径）。

### test_tick_elapsed_forward_and_wraparound

普通正差（1000→900 为 100）、零耗时，以及 `0xFFFFFFF0 → 0x10`（回绕后 32）、
`0xFFFFFFFF → 0`（完整一圈 1）的回绕减法语义。
