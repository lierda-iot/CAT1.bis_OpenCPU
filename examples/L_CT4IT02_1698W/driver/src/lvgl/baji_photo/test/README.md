# baji_photo 用例功能与使用说明

- 模块目录：`driver/src/lvgl/baji_photo/test`
- 被测 seam：`driver/src/lvgl/baji_photo/baji_photo_core_utils.{c,h}`
- 用例总数：5
- 运行全部：`make -C examples/L_CT4IT02_1698W/tests host-test MODULE=baji_photo`
- 单模块：`make -C examples/L_CT4IT02_1698W/driver/src/lvgl/baji_photo/test test`
- 单用例二进制：`driver/src/lvgl/baji_photo/test/out/baji_photo_core_utils_test`

说明：Makefile 以项目编译宏编译被测 seam（GIF/JPEG 支持开、PNG 支持关、
`BAJI_PHOTO_JPEG_MAX_COMPRESSED_SIZE=1048576`、`BAJI_PHOTO_HTTP_RAW_RGB565_VERIFY=1`），
用例同时校验"解析结果与项目宏配置一致"。

> 注：Unity 当前 `-n` 参数可解析但不跳过执行，下列「单独运行」命令会跑完整个
> 二进制并打印所有用例结果；若要只看目标用例，请配合 `grep '<用例名>.*PASS'` 使用。

## 1. test_skip_ws_and_find_key

- 被测函数：`baji_photo_skip_ws`、`baji_photo_find_json_key`
- 功能：跳过 JSON 起始空白，定位指定 key 对应的值起点。
- 输入：`"  {  \"value\" : \"abc\" , \"count\": 42 }"`，key 为 `value`、`count`。
- 期望：`skip_ws` 返回 `start+2`；`value` 指向 `"abc` 的开头双引号；`count` 指向 `4`。
- 单独运行：`driver/src/lvgl/baji_photo/test/out/baji_photo_core_utils_test -n test_skip_ws_and_find_key`

## 2. test_parse_json_string_and_numbers

- 被测函数：`baji_photo_parse_json_string`、`baji_photo_parse_json_u32`、`baji_photo_parse_json_hex_u32`
- 功能：解析字符串字段（含转义）、十进制无符号数、十六进制无符号数。
- 输入：JSON 字符串值 `"task_1234\\\"ok"`（转义后为 `task_1234\ "ok`），
  `size: 360`、`crc: "0x1A2B3C4D"`、`plain: 0x11`。
- 期望：解析均返回 0；解析出的字符串为 `task_1234\ "ok`；`size=360`；`crc=0x1A2B3C4D`；`plain=0x11`。
- 单独运行：`driver/src/lvgl/baji_photo/test/out/baji_photo_core_utils_test -n test_parse_json_string_and_numbers`

## 3. test_parse_manifest_format_matches_project_macros

- 被测函数：`baji_photo_parse_manifest_format`
- 功能：清单 format 字符串映射到枚举，并与项目 GIF/JPEG 开、PNG 关的宏配置一致。
- 输入与期望：
  - `"gif"` → `BAJI_PHOTO_FORMAT_GIF`，返回 0
  - `"JPEG"` → `BAJI_PHOTO_FORMAT_JPEG`，返回 0
  - `"png"` → 返回 -1（PNG 支持关闭）
  - `"rgb565"` → `BAJI_PHOTO_FORMAT_BJP`，返回 0
  - 无 format 字段 → 默认回退 `BAJI_PHOTO_FORMAT_BJP`，返回 0
- 单独运行：`driver/src/lvgl/baji_photo/test/out/baji_photo_core_utils_test -n test_parse_manifest_format_matches_project_macros`

## 4. test_manifest_validators_reject_bad_values

- 被测函数：`baji_photo_manifest_id_valid`、`baji_photo_manifest_remote_path_valid`
- 功能：校验 manifest ID 与远端路径的合法性，拒绝包含空格/空串/前导 `/`/`..` 等输入。
- 输入与期望：
  - `task_abcd-1234` 合法（0）；`task bad`、空串非法（-1）
  - `badge/api/image/img_1234` 合法（0）；`/badge/...`、`badge/api/../image`、
    `badge/api/image/with space` 非法（-1）
- 单独运行：`driver/src/lvgl/baji_photo/test/out/baji_photo_core_utils_test -n test_manifest_validators_reject_bad_values`

## 5. test_true_color_bounds_and_size_follow_project_limits

- 被测函数：`baji_photo_true_color_dims_valid`、`baji_photo_true_color_data_size`
- 功能：真彩图尺寸边界（1..360）与 RGB565 数据量计算（宽×高×2 字节）。
- 输入与期望：
  - 360×360、1×1 合法；0×1、361×360 非法
  - `360×360×2=259200`、`240×240×2=115200`
- 单独运行：`driver/src/lvgl/baji_photo/test/out/baji_photo_core_utils_test -n test_true_color_bounds_and_size_follow_project_limits`
