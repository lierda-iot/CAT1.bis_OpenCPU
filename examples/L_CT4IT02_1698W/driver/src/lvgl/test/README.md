# lvgl 用例功能与使用说明

- 模块目录：`driver/src/lvgl/test`
- 被测 seam：`driver/src/lvgl/lvgl_touch_logic.h`（header-only 纯逻辑）
- 用例总数：3
- 运行全部：`make -C examples/L_CT4IT02_1698W/tests host-test MODULE=lvgl`
- 单模块：`make -C examples/L_CT4IT02_1698W/driver/src/lvgl/test test`
- 单用例二进制：`driver/src/lvgl/test/out/lvgl_touch_logic_test`

说明：该 seam 覆盖触控采样分类、按压坐标覆盖、UP 事件延迟上报与坐标变换/钳制，
与 `lvgl.c` 的调用点解耦，host 编译不依赖 lvgl/liot。

> 注：Unity 当前 `-n` 参数可解析但不跳过执行，下列「单独运行」命令会跑完整个
> 二进制并打印所有用例结果；若要只看目标用例，请配合 `grep '<用例名>.*PASS'` 使用。

## 1. test_touch_latest_sample_overwrites_old_move

- 被测函数：`lvgl_touch_read_state_init`、`lvgl_touch_read_state_feed`
- 功能：连续 MOVE 采样时，最新按压坐标覆盖旧坐标。
- 输入：先喂 `(touch_cnt=1, MOVE, x=10, y=20)`，再喂 `(touch_cnt=1, MOVE, x=30, y=40)`。
- 期望：`last_x=30`、`last_y=40`、`release_pending=false`。
- 单独运行：`driver/src/lvgl/test/out/lvgl_touch_logic_test -n test_touch_latest_sample_overwrites_old_move`

## 2. test_touch_release_deferred_once_after_final_sample

- 被测函数：`lvgl_touch_read_state_feed`、`lvgl_touch_read_state_prepare_release`、`lvgl_touch_read_state_consume_release`
- 功能：UP 事件置 `release_pending`；有最终采样时延迟一拍产生 RELEASE，且只产生一次，
  消费后重置；无最终采样时不产生。
- 输入：先喂 MOVE `(12, 34)`，再喂 UP `(0, 0)`；依次调用 prepare/consume。
- 期望：`release_pending=true`；`prepare_release(true)` 返回 true 且 `release_deferred=true`；
  `consume_release()` 后 `release_deferred=false`；再 `prepare_release(false)` 返回 false。
- 单独运行：`driver/src/lvgl/test/out/lvgl_touch_logic_test -n test_touch_release_deferred_once_after_final_sample`

## 3. test_touch_transform_scale_swap_clamp

- 被测函数：`lvgl_touch_transform_point`
- 功能：按配置做 swap_xy / mirror_x / mirror_y 变换，并对超出面板范围的坐标钳制。
- 输入：`swap_xy=true`、`mirror_x=true`、`max_x=max_y=359`，原始点 `(20, 380)`。
- 期望：swap 后 `(380, 20)`，mirror_x 后 `x=359-380=-21` 钳制为 0，最终 `(0, 20)`。
- 单独运行：`driver/src/lvgl/test/out/lvgl_touch_logic_test -n test_touch_transform_scale_swap_clamp`
