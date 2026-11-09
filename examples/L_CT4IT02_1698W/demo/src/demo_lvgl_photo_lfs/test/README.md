# demo_lvgl_photo_lfs 核心逻辑单元测试

## 作用

`demo_lvgl_photo_lfs`（baji 相册外部 Flash 存储与内置资源种子）涉及大量纯字符串 / CRC / 数值逻辑。本测试把这些逻辑抽到 header-only 的
[`demo_lvgl_photo_lfs_core.h`](../demo_lvgl_photo_lfs_core.h)，在主机端直接验证，避免依赖 LVGL、外部 Flash 驱动和文件系统。

覆盖点：

- CRC-32：标准校验向量、分块更新与一次性更新一致性、空输入；
- id 合法性：NULL / 空串 / 非法字符 / 32 字符边界 / 33 字符拒绝；
- 存储格式枚举到字符串映射与最终 / 临时下载路径构造（代表格式 + 越界回退）；
- 路径缓冲截断检测与非法参数（-1）；
- rgb565 像素字节数计算（360×360）与乘法溢出拒绝（65535×65535）；
- 内置种子 id（`builtin_` 前缀）识别；
- 按 id 查找索引项（以 stride 参数兼容真实 item 结构体）。

生产代码 `demo_lvgl_photo_lfs_store.c` / `demo_lvgl_photo_lfs_seed.c` 中的同名静态函数已改为调用 core 内联实现，行为不变；若测试与真实配置（
`baji_photo_config.h` 的目录、`BAJI_PHOTO_ID_MAX_LEN`）不一致，需同步 core 宏。
本版按核心测试优化计划将 10 个用例收敛为 8 个（路径/格式全枚举合并、删除格式
round-trip 专项，防回归点不变）。

> 注：Unity 当前 `-n` 参数可解析但不跳过执行，各用例「单独运行」命令会跑完整个
> 二进制并打印所有用例结果；若要只看目标用例，请配合
> `grep '<用例名>.*PASS'` 使用。

## 如何触发

单独运行：

```sh
make -C examples/L_CT4IT02_1698W/demo/src/demo_lvgl_photo_lfs/test test
```

通过总入口运行（模块名 `photo_lfs`）：

```sh
make -C examples/L_CT4IT02_1698W/tests host-test MODULE=photo_lfs
```

运行全部 host 单元测试：

```sh
make -C examples/L_CT4IT02_1698W/tests host-test
```

冒烟门（模块内单用例快捷校验）：

```sh
make -C examples/L_CT4IT02_1698W/tests host-test-smoke MODULE=photo_lfs
```

查看用例清单：

```sh
make -C examples/L_CT4IT02_1698W/demo/src/demo_lvgl_photo_lfs/test list-tests
```

清理测试产物：

```sh
make -C examples/L_CT4IT02_1698W/demo/src/demo_lvgl_photo_lfs/test clean
```

## 用例与断言

### test_crc32_known_vector_and_empty

标准 CRC-32 校验向量：`CRC32("123456789") == 0xCBF43926`；空输入（`NULL` 指针 + 长度 0）finish 后为 `0`。

### test_crc32_incremental_update

`"hello"` + `" world"` 分两次 update 的结果与一次性 `"hello world"` 完全一致，验证增量语义。

### test_id_validity_boundaries

`NULL`、空串、含空格 / `/` / `.` 的 id 被拒绝；字母数字与 `_`、`-` 通过；恰好 32 字符通过，33 字符被拒绝。

### test_build_paths_representative_formats

验证最终存储路径与临时下载路径的代表格式：BJP `/flash/baji/photos/photo_<id>.bjp`
（tmp `/flash/baji/tmp/photo_<id>.tmp`）、JPEG `/flash/baji/jpeg/<id>.jpg`（tmp
`jpeg_<id>.tmp`）、PNG `/flash/baji/png/<id>.png`（tmp `png_<id>.tmp`）、GIF
`/flash/baji/gifs/<id>.gif`（tmp 同 BJP 规则）；未知/非法格式回退 `bjp` 路径。

### test_build_path_truncation_and_invalid

缓冲太小（如 32 字节容纳不下完整路径）返回 `-1`；恰好能容纳的路径成功；非法 id、`NULL` 输出缓冲、长度 0 均返回 `-1`。

### test_rgb565_size_and_overflow

360×360 → 259200 字节；宽 / 高为 0、输出指针为 `NULL` 返回 false；65535×65535 像素数 × 2 超过 `uint32_t` 时拒绝，验证溢出保护。

### test_builtin_id_detection

`builtin_` 前缀（含空后缀）识别为内置种子；`builtin001`、`user_001`、`NULL` 均不是。

### test_find_item_with_stride

用自定义 `{char id[33]; int value;}` 结构体数组验证按 id 查找：命中返回下标、未命中返回 `-1`，`NULL` 数组 / `NULL` id / stride 0 均返回 `-1`。
