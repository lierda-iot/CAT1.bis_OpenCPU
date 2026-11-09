# watchface 用例功能与使用说明

- 模块目录：`driver/src/lvgl/watch_earth_2015/test`
- 被测 seam：`driver/src/lvgl/watch_earth_2015/watch_earth_2015_core.h`（header-only 纯逻辑）
- 用例总数：3
- 运行全部：`make -C examples/L_CT4IT02_1698W/tests host-test MODULE=watchface`
- 单模块：`make -C examples/L_CT4IT02_1698W/driver/src/lvgl/watch_earth_2015/test test`
- 单用例二进制：`driver/src/lvgl/watch_earth_2015/test/out/watch_earth_2015_core_test`

说明：覆盖表盘时分秒指针角度换算（1/10 度），包括 12 小时制回绕、分钟/秒对指针的
阶梯贡献，纯整数运算，无 lvgl 依赖。

> 注：Unity 当前 `-n` 参数可解析但不跳过执行，下列「单独运行」命令会跑完整个
> 二进制并打印所有用例结果；若要只看目标用例，请配合 `grep '<用例名>.*PASS'` 使用。

## 1. test_hour_angle_12_hour_wrap_and_steps

- 被测函数：`watch_earth_2015_hour_angle`
- 功能：时针角度 = `(hour%12)*300 + minute*5 + second/12`（1/10 度），12 点回绕为 0。
- 输入与期望：
  - 0:00:00 / 12:00:00 → 0
  - 1:00:00 → 300；11:00:00 → 3300
  - 12:30:00 → 150（12 点回绕 + 30 分钟 × 5）
  - 1:00:48 → 304（300 + 48/12）
  - 9:30:00 → 2850（2700 + 150）
- 单独运行：`driver/src/lvgl/watch_earth_2015/test/out/watch_earth_2015_core_test -n test_hour_angle_12_hour_wrap_and_steps`

## 2. test_minute_angle_second_contribution

- 被测函数：`watch_earth_2015_minute_angle`
- 功能：分针角度 = `minute*60 + second`（1/10 度），秒针位置连续计入。
- 输入与期望：0:00 → 0；1 分 → 60；59:59 → 3599。
- 单独运行：`driver/src/lvgl/watch_earth_2015/test/out/watch_earth_2015_core_test -n test_minute_angle_second_contribution`

## 3. test_second_angle_60_steps

- 被测函数：`watch_earth_2015_second_angle`
- 功能：秒针角度 = `second*60`（1/10 度，一圈 3600）。
- 输入与期望：0 秒 → 0；1 秒 → 60；59 秒 → 3540。
- 单独运行：`driver/src/lvgl/watch_earth_2015/test/out/watch_earth_2015_core_test -n test_second_angle_60_steps`
