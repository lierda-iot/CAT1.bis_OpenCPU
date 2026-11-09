# location 用例功能与使用说明

- 模块目录：`demo/src/demo_location/test`
- 被测 seam：`demo/src/demo_location/location_core_utils.{c,h}`（与生产实现同一份源码）
- 用例总数：3
- 运行全部：`make -C examples/L_CT4IT02_1698W/tests host-test MODULE=location`
- 单模块：`make -C examples/L_CT4IT02_1698W/demo/src/demo_location/test test`
- 单用例二进制：`demo/src/demo_location/test/out/location_core_utils_test`

说明：覆盖经纬度 e7 定点数到展示/传输文本的格式化，纯字符串算术，无 cJSON/liot 依赖。

> 注：Unity 当前 `-n` 参数可解析但不跳过执行，下列「单独运行」命令会跑完整个
> 二进制并打印所有用例结果；若要只看目标用例，请配合 `grep '<用例名>.*PASS'` 使用。

## 1. test_coordinate_positive_negative_direction

- 被测函数：`location_format_coordinate`
- 功能：e7 定点坐标格式化为 `度.7位小数 方向`，负值切换 N/S 或 E/W 方向字符并取绝对值。
- 输入与期望：
  - `302741500` + N/S → `30.2741500 N`
  - `-302741500` + N/S → `30.2741500 S`
  - `1201551000` + E/W → `120.1551000 E`
  - `-1201551000` + E/W → `120.1551000 W`
- 单独运行：`demo/src/demo_location/test/out/location_core_utils_test -n test_coordinate_positive_negative_direction`

## 2. test_compact_format_no_space_negative

- 被测函数：`location_format_coordinate_compact`
- 功能：紧凑格式（无方向字符、无空格），负号自带在数值前。
- 输入与期望：`1201551000` → `120.1551000`；`-1201551000` → `-120.1551000`；`0` → `0.0000000`。
- 单独运行：`demo/src/demo_location/test/out/location_core_utils_test -n test_compact_format_no_space_negative`

## 3. test_boundary_and_truncation

- 被测函数：`location_format_coordinate`
- 功能：极端负值（INT32_MIN）不产生溢出，且输出经取绝对值截断到 7 位小数。
- 输入与期望：`INT32_MIN` + N/S → `214.7483648 S`；`-9999999` + N/S → `0.9999999 S`。
- 单独运行：`demo/src/demo_location/test/out/location_core_utils_test -n test_boundary_and_truncation`
