# demo_1khz_tone 核心逻辑单元测试

## 作用

`demo_1khz_tone`（1 kHz 正弦音 demo）中唯一纯逻辑点是音量管理：
0–100 范围校验、set/get 语义、拒绝非法值时保持原值。该逻辑被抽到
header-only 的 [`demo_tone_core.h`](../demo_tone_core.h)，生产
`demo_1khz_tone.c` 中的 static 状态与范围检查改为调用 core 内联实现，
音频设备调用（`Liot_AudioSetVolume` / `Liot_AudioSetCodecVolume`）仍在生产
侧保留，行为不变。测试在主机端直接验证，不需要 RTOS、I2S 或音频硬件。

## 如何触发

单独运行：

```sh
make -C examples/L_CT4IT02_1698W/demo/src/demo_1khz_tone/test test
```

通过总入口运行（模块名 `tone_1khz`）：

```sh
make -C examples/L_CT4IT02_1698W/tests host-test MODULE=tone_1khz
```

冒烟门（模块内单用例快捷校验）：

```sh
make -C examples/L_CT4IT02_1698W/tests host-test-smoke MODULE=tone_1khz
```

> 注：Unity 当前 `-n` 参数可解析但不跳过执行；只看单个用例请配合
> `grep '<用例名>.*PASS'` 使用。

查看用例清单 / 清理产物：

```sh
make -C examples/L_CT4IT02_1698W/demo/src/demo_1khz_tone/test list-tests
make -C examples/L_CT4IT02_1698W/demo/src/demo_1khz_tone/test clean
```

## 用例与断言

### test_volume_boundaries_and_set_get_roundtrip

音量边界 0 与 100 均可设置，且 set 后 get 返回同一值（生产默认初始值 100）。

### test_volume_out_of_range_and_updates

`-1`、`101`、`1000` 均返回 -1，且拒绝后上一有效值（100）保持不变；连续 set
30 → 55 → 0 均成功，get 依次返回最新值，验证覆盖语义；`NULL` 状态指针下
set/get 均返回 -1 且不崩溃（core 新增的保护路径）。
