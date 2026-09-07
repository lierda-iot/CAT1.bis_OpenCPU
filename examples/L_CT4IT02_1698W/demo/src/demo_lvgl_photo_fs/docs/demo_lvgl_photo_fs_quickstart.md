# L_CT4IT02_1698W demo_lvgl_photo_fs 简要使用说明

更新时间：2026-07-29

## 1. 适用范围

本文只说明当前已经落地的 `demo_lvgl_photo_fs` 用法，适用于：

- `HWDEMO_LVGL_PHOTO_FS_EN=y`
- 当前已启用的 `BJP + GIF` 场景
- 当前已验证通过的可选 `JPEG` 场景

## 2. 示例作用

`demo_lvgl_photo_fs` 用于在没有服务器的情况下，直接把内置测试图片写入 extflash，并按产品兼容方式验证：

- extflash LittleFS 挂载
- `/flash/baji` 目录结构
- `index.json` 读写
- BJP 静态图切换
- GIF 文件播放
- JPEG 文件 seed 与显示
- 重启后的 seed 校验与重建

## 3. 当前内置内容

### 3.1 BJP 图片

- `builtin_001` ~ `builtin_005`
- 对应 `landscape_1_data.c` ~ `landscape_5_data.c`

### 3.2 GIF 图片

- `builtin_gif_001`
  - `baji_gif_seed_5_data`
- `builtin_gif_002`
  - `saa_360_360_gif_data`
- `builtin_gif_003`
  - `saq_360_360_gif_data`

### 3.3 JPEG 图片

- `builtin_jpg_001`
  - `landscape_6_360_360_jpg_data`
- `builtin_jpg_002`
  - `landscape_7_360_360_jpg_data`
- `builtin_jpg_003`
  - `landscape_8_360_360_jpg_data`

说明：

- 对应源图位于 `assets/images/landscape_6~8_360_360.jpg`
- 当前实机验证通过的 JPG 必须为 `baseline JPEG`

## 4. 编译方式

推荐命令：

```bash
make all PROJECT=L_CT4IT02_1698W MODEM=NT26F9D0 MODEMPKG=F9D_A \
    CODE_REGION_ADJUST=0 \
    BUILD_MODE=demo \
    HWDEMO_EXTFLASH_P25Q64_EN=n \
    HWDEMO_EXTFLASH_FS_P25Q64_EN=n \
    HWDEMO_LVGL_PHOTO_FS_EN=y \
    DEMO_LVGL_PHOTO_FS_ENABLE_JPEG=1 \
    HWDEMO_SALARY_CALCULATOR_EN=n \
    HWDEMO_MP3_EN=n
```

相关开关：

- `DEMO_LVGL_PHOTO_FS_ENABLE_GIF=1`
- `DEMO_LVGL_PHOTO_FS_ENABLE_JPEG=1`
- `DEMO_LVGL_PHOTO_FS_FORCE_RESEED=0`

如需每次启动都重写内置资源，可额外加：

```bash
DEMO_LVGL_PHOTO_FS_FORCE_RESEED=1
```

说明：

- 推荐显式关闭 `HWDEMO_SALARY_CALCULATOR_EN` 和 `HWDEMO_MP3_EN`，避免 demo 模式下同时拉起其他功能干扰启动观察。
- 如仅快速复现当前用户验证通过的组合，优先使用上面的完整命令，不要自行删减开关。

## 5. 运行流程

上电进入 demo 后，示例会自动执行：

1. 初始化 extflash 电源和 SPI
2. 挂载 LittleFS
3. 检查 `/flash/baji`
4. 校验或写入内置 seed
5. 读取 `index.json`
6. 打开第一张图片

## 6. 页面操作

- 单击：下一张
- 左右滑动：前后切换
- 长按约 1 秒：重新执行 seed

如果当前正在 reseed，页面会暂时阻止切图。

## 7. 预期结果

首次烧录、extflash 为空时，预期行为如下：

- 上电后自动完成 extflash 初始化、LittleFS 挂载和 seed 校验/写入
- `/flash/baji` 下生成 `photos/`、`gifs/`、`tmp/` 和 `index.json`
- 页面自动打开首张内置图片
- 单击或滑动后可以稳定切换到下一张/上一张
- GIF 资源切换到对应条目时可以正常播放
- 切换到 JPG 条目时可以正常显示 JPG

若 extflash 中已经存在兼容的产品数据，预期行为如下：

- 启动时不会重复写入全部资源
- `index.json` 校验通过后直接加载现有图片列表
- 页面可直接用于验证产品格式的照片切换、GIF 播放与 JPG 显示

当前用户已按以下命令实机验证通过：

```bash
make all PROJECT=L_CT4IT02_1698W MODEM=NT26F9D0 MODEMPKG=F9D_A \
    CODE_REGION_ADJUST=0 \
    BUILD_MODE=demo \
    HWDEMO_LVGL_PHOTO_FS_EN=y \
    DEMO_LVGL_PHOTO_FS_ENABLE_JPEG=1 \
    HWDEMO_SALARY_CALCULATOR_EN=n \
    HWDEMO_MP3_EN=n
```

## 8. 关键日志

重点看以下前缀：

- `[demo_lvgl_photo_fs]`
- `[demo_lvgl_photo_fs_store]`
- `[demo_lvgl_photo_fs_media]`

常见有效日志：

- `start items=...`
- `seed ok ...`
- `seed ret=...`

异常时重点关注：

- `Load failed ...`
- mount / open / save index 相关失败日志

## 9. 数据位置

当前 demo 使用与产品兼容的 extflash 目录：

- 根目录：`/flash/baji`
- 照片目录：`/flash/baji/photos`
- GIF 目录：`/flash/baji/gifs`
- JPEG 目录：`/flash/baji/jpeg`
- 临时目录：`/flash/baji/tmp`
- 索引文件：`/flash/baji/index.json`

因此如果 extflash 中已经有产品侧生成的兼容文件，当前 demo 也可以直接加载验证。

## 10. 异常现象与排障

### 10.1 上电后直接复位

该问题在 2026-07-29 已修复，根因是 `demo_lvgl_photo_fs_seed_run()` 启动阶段在任务栈上分配了两组大数组，导致启动线程栈溢出。

如果后续再次出现类似现象，优先检查：

- 是否拉入了未合入修复的旧版本 `demo_lvgl_photo_fs_seed.c`
- 启动阶段是否又新增了大数组、文件缓冲等栈对象
- extflash 电源、SPI、LittleFS 初始化日志是否在复位前已输出
- demo 模式下是否同时使能了多个重型功能

### 10.2 页面打不开或切图失败

优先检查：

- `demo_lvgl_photo_fs_store_mount()` 是否成功
- `/flash/baji/index.json` 是否存在且内容完整
- `photos/`、`gifs/` 目录下文件是否与索引一致
- 日志中是否出现 `Load failed`、`open failed`、`save index failed`

### 10.3 强制重建 extflash 内容

若怀疑 extflash 中残留旧数据，可通过以下方式强制重写：

```bash
DEMO_LVGL_PHOTO_FS_FORCE_RESEED=1
```

也可在页面长按约 1 秒重新执行 reseed。

### 10.4 JPG 不显示或 reseed 后未看到 JPG

优先区分两类情况：

1. reseed 成功，但页面只是重新打开了索引 `0`
2. JPG 本身未通过 `JpegD_DecodeInfo()` 校验

当前已确认的格式约束：

- `JpegD` 输入是原始 JPEG 压缩数据
- 当前测试链路要求 `baseline JPEG`
- `progressive JPEG` 会导致 seed 校验或显示失败

建议排查顺序：

- 用 `file <jpg>` 确认输出中是否包含 `baseline`
- 若不是 baseline，先转换为 `SOF0`
- 转换后重新生成 `*_jpg_data.c`
- 重新编译并长按 reseed
- reseed 完成后继续手动切换到 JPG 条目验证
