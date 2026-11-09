# demo_mp3_standalone 用例功能与使用说明

- 模块目录：`demo/src/demo_mp3_standalone/test`
- 被测 seam：`demo/src/demo_mp3_standalone/demo_mp3_standalone_core.h`
  （与生产实现同一份源码，header-only）
- 用例总数：2
- 运行全部：`make -C examples/L_CT4IT02_1698W/tests host-test MODULE=mp3_standalone`
- 冒烟门：`make -C examples/L_CT4IT02_1698W/tests host-test-smoke MODULE=mp3_standalone`
  （用例 `test_volume_boundaries_and_out_of_range`，跑全量后 grep 该用例 PASS + OK；
  计划文档 6.1 中写作 `test_volume_zero_and_full_boundaries`，落地为合并后的
  `test_volume_boundaries_and_out_of_range`）
- 单模块：`make -C examples/L_CT4IT02_1698W/demo/src/demo_mp3_standalone/test test`
- 单用例二进制：`demo/src/demo_mp3_standalone/test/out/demo_mp3_standalone_core_test`

说明：覆盖 AT+MP3VOL 音量设置中“用户音量(0-100) → 硬件安全音量(0-70)”的纯
整数映射逻辑，无 liot 音频依赖。本版按核心测试优化计划将 4 个用例收敛为 2 个
（边界+越界合并、rounding+安全上限合并），防回归点不变。

> 注：Unity 当前 `-n` 参数可解析但不跳过执行，下列「单独运行」命令会跑完整个二进制
> 并打印所有用例结果；若要只看目标用例，请配合 `grep '<用例名>.*PASS'` 使用。

## 1. test_volume_boundaries_and_out_of_range

- 被测函数：`demo_mp3_standalone_hw_volume`
- 功能：用户音量 0/100 两个边界值映射正确；越界用户音量（负值、大于 100）返回
  -1，与生产 `standalone_mp3_set_volume` 提前拒绝一致。
- 输入与期望：`0` → `0`；`100` → `70`；`-1` / `101` / `1000` → `-1`。
- 单独运行：`demo/src/demo_mp3_standalone/test/out/demo_mp3_standalone_core_test -n test_volume_boundaries_and_out_of_range`

## 2. test_volume_rounding_scale_and_safe_cap

- 被测函数：`demo_mp3_standalone_hw_volume`
- 功能：`(volume * 70 + 50) / 100` 的取整与缩放正确，任意合法用户音量映射结果
  不超过硬件安全上限 70。
- 输入与期望：`1` → `1`；`20` → `14`；`50` → `35`；`90` → `63`；`99` → `69`。
- 单独运行：`demo/src/demo_mp3_standalone/test/out/demo_mp3_standalone_core_test -n test_volume_rounding_scale_and_safe_cap`
