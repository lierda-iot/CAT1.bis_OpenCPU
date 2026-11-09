# demo_gnss 用例功能与使用说明

- 模块目录：`demo/src/demo_gnss/test`
- 被测 seam：`demo/src/demo_gnss/demo_gnss_core.h`（与生产实现同一份源码，header-only）
- 用例总数：5
- 运行全部：`make -C examples/L_CT4IT02_1698W/tests host-test MODULE=gnss`
- 冒烟门：`make -C examples/L_CT4IT02_1698W/tests host-test-smoke MODULE=gnss`
  （用例 `test_coordinate_null_empty_rejected`，跑全量后 grep 该用例 PASS + OK）
- 单模块：`make -C examples/L_CT4IT02_1698W/demo/src/demo_gnss/test test`
- 单用例二进制：`demo/src/demo_gnss/test/out/demo_gnss_core_test`

说明：覆盖 CC1161W NMEA `ddmm.mmmmm` 坐标到 e7 定点数的解析（半球取负、非法
输入拒绝、分钟超界拒绝、小数截断）以及 NMEA 句子逗号字段拆分，纯字符串/整数
逻辑，无 liot/UART 依赖。本版按核心测试优化计划将 7 个用例收敛为 5 个
（正/负半球合并、非法半球与分钟越界合并，防回归点不变）。

> 注：Unity 当前 `-n` 参数可解析但不跳过执行，下列「单独运行」命令会跑完整个二进制
> 并打印所有用例结果；若要只看目标用例，请配合 `grep '<用例名>.*PASS'` 使用。

## 1. test_coordinate_positive_and_negative_hemisphere

- 被测函数：`gnss_coordinate_to_e7`
- 功能：N/E 半球正常坐标换算为 e7 定点（度+分/60，7 位小数），S/W 半球结果取负。
- 输入与期望：`3103.33427 N` → `310555711`；`12015.55123 E` → `1202591871`；
  `3103.33427 S` → `-310555711`；`12015.55123 W` → `-1202591871`。
- 单独运行：`demo/src/demo_gnss/test/out/demo_gnss_core_test -n test_coordinate_positive_and_negative_hemisphere`

## 2. test_coordinate_invalid_hemisphere_and_minutes

- 被测函数：`gnss_coordinate_to_e7`
- 功能：非 N/E/S/W 半球（含小写）返回 false 且不改写输出值；分钟字段 `>= 60`
  （如 `31060.12345`）按非法 NMEA 拒绝，`31059.99999` 为合法（保留原生产语义断言）。
- 单独运行：`demo/src/demo_gnss/test/out/demo_gnss_core_test -n test_coordinate_invalid_hemisphere_and_minutes`

## 3. test_coordinate_null_empty_rejected

- 被测函数：`gnss_coordinate_to_e7`
- 功能：NULL/空串/非数字输入返回 false，输出指针为 NULL 时不写。
- 单独运行：`demo/src/demo_gnss/test/out/demo_gnss_core_test -n test_coordinate_null_empty_rejected`

## 4. test_coordinate_fraction_truncated_to_7_digits

- 被测函数：`gnss_coordinate_to_e7`
- 功能：小数位超过 7 位只保留前 7 位（截断而非四舍五入）。
- 输入与期望：`3103.60123456 N` → `310600205`；`3103.60 N` → `310600000`。
- 单独运行：`demo/src/demo_gnss/test/out/demo_gnss_core_test -n test_coordinate_fraction_truncated_to_7_digits`

## 5. test_split_fields_nmea_sentence_and_empty

- 被测函数：`gnss_split_fields`
- 功能：按逗号拆分 NMEA 句子，返回字段数；空字段保留为空串；NULL/空行返回 0。
- 单独运行：`demo/src/demo_gnss/test/out/demo_gnss_core_test -n test_split_fields_nmea_sentence_and_empty`
