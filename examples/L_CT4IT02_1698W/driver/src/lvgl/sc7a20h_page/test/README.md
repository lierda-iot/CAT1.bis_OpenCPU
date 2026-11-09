# attitude 用例功能与使用说明

- 模块目录：`driver/src/lvgl/sc7a20h_page/test`
- 被测 seam：`driver/src/lvgl/sc7a20h_page_core.h`（header-only 纯逻辑）
- 用例总数：3
- 运行全部：`make -C examples/L_CT4IT02_1698W/tests host-test MODULE=attitude`
- 单模块：`make -C examples/L_CT4IT02_1698W/driver/src/lvgl/sc7a20h_page/test test`
- 单用例二进制：`driver/src/lvgl/sc7a20h_page/test/out/sc7a20h_page_core_test`

说明：覆盖 SC7A20H 姿态页中坐标钳制、加速度到屏幕坐标的线性映射与一阶滤波，
生产调用点显式传滤波状态，行为不变。

> 注：Unity 当前 `-n` 参数可解析但不跳过执行，下列「单独运行」命令会跑完整个
> 二进制并打印所有用例结果；若要只看目标用例，请配合 `grep '<用例名>.*PASS'` 使用。

## 1. test_clamp_inside_lower_upper_bounds

- 被测函数：`sc7a20h_clamp_int`
- 功能：把数值钳制到 [min, max]。
- 输入与期望：`clamp(5, 0, 10)=5`；`clamp(-1, 0, 10)=0`；`clamp(11, 0, 10)=10`。
- 单独运行：`driver/src/lvgl/sc7a20h_page/test/out/sc7a20h_page_core_test -n test_clamp_inside_lower_upper_bounds`

## 2. test_map_dot_axis_center_and_clamp

- 被测函数：`sc7a20h_map_dot_axis`
- 功能：`center + accel*radius/range` 映射到屏幕轴，并钳制到 [min_pos, max_pos]。
- 输入与期望（center=180、radius=130、range=500、min=9、max=351）：
  - accel=0 → 180；accel=500 → 310；accel=-500 → 50
  - accel=-1000 → 钳制到 9；accel=1000 → 钳制到 351
- 单独运行：`driver/src/lvgl/sc7a20h_page/test/out/sc7a20h_page_core_test -n test_map_dot_axis_center_and_clamp`

## 3. test_filter_axis_weighted_average

- 被测函数：`sc7a20h_filter_axis`
- 功能：`(prev*3 + target) / 4` 一阶滤波（整数除法，向 0 取整）。
- 输入与期望：`(0, 0)=0`；`(0, 10)=2`；`(10, 0)=7`；`(180, 180)=180`；`(0, 1)=0`。
- 单独运行：`driver/src/lvgl/sc7a20h_page/test/out/sc7a20h_page_core_test -n test_filter_axis_weighted_average`
