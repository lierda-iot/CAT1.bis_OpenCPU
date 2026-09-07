# L_CT4IT02_1698W LVGL Photo FS 示例说明

更新时间：2026-07-29

## 1. 示例文件

- 源码目录：`examples/L_CT4IT02_1698W/demo/src/demo_lvgl_photo_fs/`
- 入口函数：`liot_lvgl_photo_fs_demo_thread(void *argv)`
- 文档适用开关：`HWDEMO_LVGL_PHOTO_FS_EN=y`

## 2. 示例用途

该示例用于独立验证：

- extflash LittleFS 挂载
- 与产品一致的 `/flash/baji` 数据目录
- BJP 静态照片显示
- GIF 文件播放
- 重启后 index 与文件重建 / 校验

当前已经完成 `BJP + GIF + JPEG` 接入。

`PNG` 仍预留在当前示例目录与 extflash 数据结构中，后续继续复用该路径接入。

当前目标是让 demo 与产品共用同一套 extflash 文件布局，这样在服务器未接通时，也可以直接通过 demo seed / 灌图验证产品照片切换链路。

## 3. 当前目录布局

- 代码目录：`examples/L_CT4IT02_1698W/demo/src/demo_lvgl_photo_fs/`
- 资源目录：`examples/L_CT4IT02_1698W/demo/src/demo_lvgl_photo_fs/assets/`
- extflash 根目录：`/flash/baji`
- 索引文件：`/flash/baji/index.json`
- 照片文件：`/flash/baji/photos/photo_<id>.bjp`
- GIF 文件：`/flash/baji/gifs/<id>.gif`
- JPEG 文件：`/flash/baji/jpeg/<id>.jpg`
- PNG 文件：`/flash/baji/png/<id>.png`
- 临时文件目录：`/flash/baji/tmp`

`baji_photo` 产品目录下原有 `driver/src/lvgl/baji_photo/assets/` 已迁移到本示例目录，产品默认不再依赖这批内置资源；但 demo 写入 extflash 的目录布局、命名规则和 index 文件格式保持兼容产品当前实现。

## 4. 当前内置资源

### 4.1 BJP 资源

- `assets/landscape_1_data.c`
- `assets/landscape_2_data.c`
- `assets/landscape_3_data.c`
- `assets/landscape_4_data.c`
- `assets/landscape_5_data.c`

### 4.2 GIF 资源

- `assets/saa_360_360_gif_data.c`
- `assets/saq_360_360_gif_data.c`
- 原始 GIF：`assets/images/SAA_360_360.gif`
- 原始 GIF：`assets/images/SAQ_360_360.gif`

### 4.3 资源生成辅助脚本

- `examples/L_CT4IT02_1698W/tools/lv9_to_lv8_landscape.py`
- `assets/tools/gif_to_c_array.py`
- `assets/tools/LVGLImage.py`
- `assets/tools/GIF_C_ARRAY_USAGE.md`

### 4.4 JPEG 资源

- `assets/landscape_6_360_360_jpg_data.c`
- `assets/landscape_7_360_360_jpg_data.c`
- `assets/landscape_8_360_360_jpg_data.c`
- 原始 JPG：`assets/images/landscape_6_360_360.jpg`
- 原始 JPG：`assets/images/landscape_7_360_360.jpg`
- 原始 JPG：`assets/images/landscape_8_360_360.jpg`

说明：

- 当前设备侧 `JpegD` 实测要求测试资源使用 `baseline JPEG`
- progressive JPEG 需要先转换为 `SOF0`

## 5. 编译开关

板级宏：

- `HWDEMO_LVGL_PHOTO_FS_EN`

格式开关：

- `DEMO_LVGL_PHOTO_FS_ENABLE_GIF`
- `DEMO_LVGL_PHOTO_FS_ENABLE_JPEG`
- `DEMO_LVGL_PHOTO_FS_ENABLE_PNG`
- `DEMO_LVGL_PHOTO_FS_FORCE_RESEED`

当前默认值：

- `GIF=1`
- `JPEG=0`
- `PNG=0`
- `FORCE_RESEED=0`

## 6. 建议编译方式

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

说明：

- `DEMO_LVGL_PHOTO_FS_ENABLE_JPEG` 当前默认仍为 `0`
- 进行 JPG seed / 显示验证时，建议显式传入 `DEMO_LVGL_PHOTO_FS_ENABLE_JPEG=1`

## 7. 当前交互方式

- 上电后自动初始化 extflash / LittleFS
- 启动时自动执行一次 seed 校验
- 单击：切下一张
- 左右手势：前后切换
- 长按：重新 seed 内置资源

注意：

- 长按 reseed 完成后，页面当前会重新打开索引 `0`
- 因此 reseed 后若要看到 JPG，仍需手动切换到 JPG 条目

## 8. 当前预期日志

重点日志前缀：

- `[demo_lvgl_photo_fs]`
- `[demo_lvgl_photo_fs_store]`
- `[demo_lvgl_photo_fs_media]`

启动时应重点关注：

- `start items=...`
- `seed ok ...`
- `seed ret=...`
- `Load failed ...`（仅异常时出现）

如果 extflash 中已经存在产品侧 seed 或下载留下的 `index.json + photos/gifs`，当前 demo 应可直接加载并切图。

如果使能了 `JPEG`，则还会创建并校验：

- `/flash/baji/jpeg`
- 对应 `.jpg` 文件

## 9. 当前边界

- 当前不复用 `baji_photo` 页面与产品入口
- 当前已验证 `BJP + GIF + JPEG`
- `PNG` 枚举与目录模型已预留，但解码链路尚未启用
- 当前 JPG 测试图要求为 `baseline JPEG`，`progressive JPEG` 不保证可用
- `assets/baji_gif_seed_5_data.c` 仅保留为历史兼容占位，当前示例未接入
