# demo_sc7a20h 用例功能与使用说明

- 模块目录：`demo/src/demo_sc7a20h/test`
- 被测 seam：`demo/src/demo_sc7a20h/demo_sc7a20h_core.h`（与生产实现同一份源码，header-only）
- 用例总数：2
- 运行全部：`make -C examples/L_CT4IT02_1698W/tests host-test MODULE=sc7a20h`
- 冒烟门：`make -C examples/L_CT4IT02_1698W/tests host-test-smoke MODULE=sc7a20h`
  （用例 `test_threshold_level_mg_and_boundaries`，跑全量后 grep 该用例 PASS + OK；
  计划文档 6.1 中写作 `test_threshold_boundaries_zero_and_max`，落地为合并后的
  `test_threshold_level_mg_and_boundaries`）
- 单模块：`make -C examples/L_CT4IT02_1698W/demo/src/demo_sc7a20h/test test`
- 单用例二进制：`demo/src/demo_sc7a20h/test/out/demo_sc7a20h_core_test`

说明：覆盖 SC7A20H demo 的灵敏度阈值换算（level*32 mg，含 0/128 边界）、摇晃
计数达标判定、点击次数匹配判定，纯整数/布尔逻辑，无 lvgl/liot/硬件依赖。本版
按核心测试优化计划将 4 个用例收敛为 2 个（阈值线性换算 2 用例合并、摇晃/点击
平凡布尔比较合并），防回归点不变。

> 注：Unity 当前 `-n` 参数可解析但不跳过执行，下列「单独运行」命令会跑完整个二进制
> 并打印所有用例结果；若要只看目标用例，请配合 `grep '<用例名>.*PASS'` 使用。

## 1. test_threshold_level_mg_and_boundaries

- 被测函数：`sc7a20h_demo_threshold_mg`
- 功能：灵敏度档位换算为加速度阈值 mg（`level * 32`），含 0 档关闭级联与
  128 档满量程边界。
- 输入与期望：`1` → `32`；`40` → `1280`；`108` → `3456`（与生产 click 配置注释
  一致）；`0` → `0`；`128` → `4096`。
- 单独运行：`demo/src/demo_sc7a20h/test/out/demo_sc7a20h_core_test -n test_threshold_level_mg_and_boundaries`

## 2. test_shake_and_click_hit

- 被测函数：`sc7a20h_demo_shake_hit`、`sc7a20h_demo_click_hit`
- 功能：摇晃次数累计达到配置次数判定为触发（保留生产 `>=` 语义）；传感器上报
  点击次数等于配置次数时触发（保留生产 `==` 语义）。
- 输入与期望：摇晃 `(0,1)` false；`(1,1)` true；`(3,2)` true；`(0,0)` true。
  点击 `(2,2)` true；`(1,2)` false；`(0,2)` false；`(3,2)` false。
- 单独运行：`demo/src/demo_sc7a20h/test/out/demo_sc7a20h_core_test -n test_shake_and_click_hit`
