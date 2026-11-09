# L_CT4IT02_1698W 核心功能单元测试阶段性总结

- 分支：`L_CT4IT02_1698W-unit-test`
- 总结日期：2026-10-09
- 覆盖范围：`examples/L_CT4IT02_1698W/tests` 统一入口下的 14 个模块、53 个 Unity host 用例
  （核心测试优化后；冒烟门 14 个，见 §4）
  （测试用例已就近迁移到各生产 seam 目录的 `test/` 子目录，见 §4）

## 1. 阶段性成果

1. Unity 测试框架以 `components/thirdparty/unity` 入库，默认构建不使能，不影响 app/demo 固件编译。
2. 对 12 个核心功能（baji_photo，mp3 / salary / location / attitude / watchface ，
   gnss / sc7a20h，mp3_standalone / photo_lfs / extflash_p25q64 / xiaozhi /
   tone_1khz）抽取与生产实现同一份的纯逻辑 seam，并通过 host 单测验证。
3. `tests/Makefile` 提供一键入口：全部模块、指定模块、列出用例。
4. 固件组合编译验证 4 组均 EXIT=0（见第 3 节）。

当前测试覆盖的是**真实生产 seam 源码**（`demo_mp3_core.c`、`salary_core_utils.c`、
`location_core_utils.c`、`sc7a20h_page_core.h`、`watch_earth_2015_core.h`、
`baji_photo_core_utils.c`、`lvgl_touch_logic.h`），不是测试副本。

## 2. 测试结果

`make -C examples/L_CT4IT02_1698W/tests host-test` 执行结果：

| 模块 | 用例数 | 结果 |
| --- | --- | --- |
| baji_photo | 5 | PASS |
| lvgl | 3 | PASS |
| mp3 | 6 | PASS |
| salary | 5 | PASS |
| location | 3 | PASS |
| attitude | 3 | PASS |
| watchface | 3 | PASS |
| gnss | 5 | PASS |
| sc7a20h | 2 | PASS |
| mp3_standalone | 2 | PASS |
| photo_lfs | 8 | PASS |
| extflash_p25q64 | 2 | PASS |
| xiaozhi | 4 | PASS |
| tone_1khz | 2 | PASS |
| **合计** | **53** | **全部 PASS，0 失败，0 忽略，EXIT=0** |

## 3. 固件编译验证（均 EXIT=0）

| 配置 | FLASH 占用 | RAM 占用 |
| --- | --- | --- |
| `BUILD_MODE=app` | 2241176 B | 31304 B |
| `BUILD_MODE=demo` | 2564432 B | 141160 B |
| `BUILD_MODE=app APP_SALARY_EN=y APP_MAP_EN=y`（A，其余默认 y） | 3239128 B | 32344 B |
| A + `APP_MP3_EN=n`（C） | 2762240 B | 32232 B |
| A + `APP_WATCHFACE_EN=n`（D） | 2928744 B | 32312 B |
| A + `APP_ATTITUDE_EN=n`（E） | 3170320 B | 32200 B |
| A + `APP_BAJI_EN=n`（F） | 3030808 B | 5968 B |
| A + 四项全关（B） | 2173920 B | 5696 B |

观察项结论（本阶段不改业务代码）：此前“叠加 mp3/watchface/attitude 开关后尺寸无增量”
是假象——`config` 默认值即 `APP_WATCHFACE_EN=y APP_ATTITUDE_EN=y APP_MP3_EN=y
APP_BAJI_EN=y`，显式传 `=y` 与默认集合一致。改用显式 `=n` 后开关全部生效：
mp3 走 `HWDEMO_MP3_EN/BUILD_THPART_MP3_ENABLE` 联动（-477,064 B）；watchface
（-310,384 B）、attitude（-68,808 B）、baji（-208,320 B）经 `-DAPP_*_EN=0` 的 `#if`
守卫与 `--gc-sections` 丢弃未引用段；四项全关共减 1,065,208 B，RAM 由 32344 B 降至
5696 B。watch/attitude/baji 源文件仍全部编入 `liblvgl.a`，仅靠链接器裁剪（详见计划
文档 11.2 节机制说明与优化建议）。

## 4. 如何再次触发与指定单元测试

全部用例：

```sh
make -C examples/L_CT4IT02_1698W/tests host-test
```

指定一个或多个模块（空格分隔，需引号）：

```sh
make -C examples/L_CT4IT02_1698W/tests host-test MODULE=mp3
make -C examples/L_CT4IT02_1698W/tests host-test MODULE='mp3 salary watchface'
```

列出所有用例：

```sh
make -C examples/L_CT4IT02_1698W/tests list-tests
```

单模块直接运行：

```sh
make -C examples/L_CT4IT02_1698W/demo/src/demo_mp3/test test
```

清理后重编单模块（验证干净构建）：

```sh
make -C examples/L_CT4IT02_1698W/demo/src/demo_mp3/test clean
make -C examples/L_CT4IT02_1698W/demo/src/demo_mp3/test test
```

冒烟门（14 个模块各 1 个最高价值用例，提交前秒级跑完）：

```sh
make -C examples/L_CT4IT02_1698W/tests host-test-smoke
make -C examples/L_CT4IT02_1698W/tests host-test-smoke MODULE=mp3
```

冒烟门用例清单：

| 模块 | 冒烟门用例 |
| --- | --- |
| baji_photo | `test_manifest_validators_reject_bad_values` |
| lvgl | `test_touch_release_deferred_once_after_final_sample` |
| mp3 | `test_duration_estimate_single_frame_cbr` |
| salary | `test_earned_cent_window_and_rate` |
| location | `test_coordinate_positive_negative_direction` |
| attitude | `test_map_dot_axis_center_and_clamp` |
| watchface | `test_hour_angle_12_hour_wrap_and_steps` |
| gnss | `test_coordinate_null_empty_rejected` |
| sc7a20h | `test_threshold_level_mg_and_boundaries` |
| mp3_standalone | `test_volume_boundaries_and_out_of_range` |
| photo_lfs | `test_crc32_known_vector_and_empty` |
| extflash_p25q64 | `test_tick_elapsed_forward_and_wraparound` |
| xiaozhi | `test_parse_ws_url_host_port_path` |
| tone_1khz | `test_volume_boundaries_and_set_get_roundtrip` |

> 说明：冒烟门为「跑全量二进制 + grep 目标用例 PASS + 最终 OK」的兜底校验（Unity
> 当前 `-n` 参数可解析但不跳过执行，无法只运行单用例，故不依赖 `-n` 过滤）。

运行单个用例（Unity `-n` 参数）：

```sh
examples/L_CT4IT02_1698W/driver/src/lvgl/watch_earth_2015/test/out/watch_earth_2015_core_test -n test_hour_angle_12_hour_wrap_and_steps
```

> 限制：Unity 当前 `-n` 只解析参数、不跳过执行，因此上述命令会跑完整个二进制并打印
> 所有用例结果；想看单个用例结论请配合 `grep '<用例名>.*PASS'`。

每个用例的单独运行方式见各模块文档：

- [baji_photo 用例文档](../driver/src/lvgl/baji_photo/test/README.md)
- [lvgl 用例文档](../driver/src/lvgl/test/README.md)
- [mp3 用例文档](../demo/src/demo_mp3/test/README.md)
- [salary 用例文档](../demo/src/demo_salary_calculator/test/README.md)
- [location 用例文档](../demo/src/demo_location/test/README.md)
- [attitude 用例文档](../driver/src/lvgl/sc7a20h_page/test/README.md)
- [watchface 用例文档](../driver/src/lvgl/watch_earth_2015/test/README.md)
- [gnss 用例文档](../demo/src/demo_gnss/test/README.md)
- [sc7a20h demo 用例文档](../demo/src/demo_sc7a20h/test/README.md)
- [mp3_standalone 用例文档](../demo/src/demo_mp3_standalone/test/README.md)
- [photo_lfs 用例文档](../demo/src/demo_lvgl_photo_lfs/test/README.md)
- [extflash_p25q64 用例文档](../demo/src/demo_extflash_p25q64/test/README.md)
- [xiaozhi 用例文档](../demo/src/demo_xiaozhi/test/README.md)
- [tone_1khz 用例文档](../demo/src/demo_1khz_tone/test/README.md)

> 说明：模块测试用例已迁移到各生产 seam 所在目录的 `test/` 子目录（如
> `demo/src/demo_mp3/test`、`driver/src/lvgl/watch_earth_2015/test`），`tests/` 目录仅保留
> 统一入口 `Makefile` 与本总结文档。

## 5. 边界与未处理项

- 核心测试优化只改测试目录（14 个模块的 `test/` 与 `tests/` 入口）与两份文档；
  未改任何生产 core seam、config 与固件 Makefile。
- `6a723a0b` 的 Makefile 提交中混入了既有 muse_xiaozhi hunk（提交前已说明，未回退）。
- 测试断言修正记录：mp3 暂停进度、salary 解析/收入折算、attitude 映射/滤波、
  watchface 时角计算等为断言本身写错，core 实现未修改（详见计划文档 11.3 节）。
- gnss `gnss_split_fields` 对空串返回 0 是对原生产行为的兼容修正（原实现空串返回 1）。
- xiaozhi 用例修正：空主机返回 `-2`（原 README 先记为 -1，实际生产路径先做 host
  长度检查）、`"hello"` FNV-1a 标准向量为 `0x4F9F2CAB`；core 实现未修改。
- tone_1khz / mp3_standalone / extflash / photo_lfs 的 core 新增防御性 `NULL`
  保护（生产调用均为非 NULL，属兼容增强，README 已注明）。

## 6. 后续待办
