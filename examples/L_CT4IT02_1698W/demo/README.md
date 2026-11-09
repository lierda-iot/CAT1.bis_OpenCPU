# L_CT4IT02_1698W Demo 使用说明索引

更新时间：2026-10-10

## 1. 定位

本目录包含 `L_CT4IT02_1698W` 各外设/功能 demo 的源码与使用说明。说明文档与源码同目录存放，纯逻辑单元测试说明位于对应 `test/README.md`。

## 2. 通用构建命令

```bash
make all PROJECT=L_CT4IT02_1698W MODEM=NT26F9D0 MODEMPKG=F9D_A BUILD_MODE=demo
```

- 所有命令均为单行、无 `\` 续行，可直接整行拷贝执行。
- 只验证部分 demo 时，在命令后显式传对应板级开关（见下表）；`make all` 自带 clean，切换开关后重新完整编译。
- `HWDEMO_SOUND_ES8375_EN` 在 config 默认 `n`，声音三选一默认关闭；需要声音 demo 时显式传 `=y`。
- 各 demo 的完整编译命令以对应说明文档为准。

## 3. Demo 清单

| Demo | 功能 | 入口线程 | 板级开关 | 说明文档 |
| --- | --- | --- | --- | --- |
| demo_lcd | 圆屏 LCD + 触摸链路 | `liot_lcd_demo_thread` | `HWDEMO_LCD_EN` | [demo_lcd.md](src/demo_lcd/docs/demo_lcd.md) |
| demo_sc7a20h | G-Sensor 摇晃/双击 + LVGL 可视化 | `liot_sc7a20h_demo_thread` | `HWDEMO_SC7A20H_EN` | [demo_sc7a20h.md](src/demo_sc7a20h/docs/demo_sc7a20h.md) |
| demo_gnss | CC1161W UART / NMEA / AGNSS | `liot_gnss_demo_thread` | `HWDEMO_GNSS_EN` | [demo_gnss.md](src/demo_gnss/docs/demo_gnss.md) |
| demo_extflash_p25q64 | extflash raw 擦写 / 扇区 RMW | `liot_extflash_p25q64_demo_thread` | `HWDEMO_EXTFLASH_P25Q64_EN` | [demo_extflash_p25q64.md](src/demo_extflash_p25q64/docs/demo_extflash_p25q64.md) |
| demo_extflash_p25q64_lfs | LittleFS 挂载 / 文件 smoke | `liot_extflash_p25q64_lfs_demo_thread` | `HWDEMO_EXTFLASH_P25Q64_LFS_EN` | [demo_extflash_p25q64_lfs.md](src/demo_extflash_p25q64_lfs/docs/demo_extflash_p25q64_lfs.md) |
| demo_lvgl_photo_lfs | 照片 seed / 切图 / GIF / JPEG | `liot_lvgl_photo_lfs_demo_thread` | `HWDEMO_LVGL_PHOTO_LFS_EN` | [说明](src/demo_lvgl_photo_lfs/docs/demo_lvgl_photo_lfs.md) / [快速上手](src/demo_lvgl_photo_lfs/docs/demo_lvgl_photo_lfs_quickstart.md) |
| demo_salary_calculator | 工资计算 + 金币音效 | `liot_salary_calculator_demo_thread` | `HWDEMO_SALARY_CALCULATOR_EN` | [README.md](src/demo_salary_calculator/README.md) |
| demo_mp3 | LVGL MP3 播放器 | `liot_mp3_demo_thread` | `HWDEMO_MP3_EN` | [demo_mp3.md](src/demo_mp3/docs/demo_mp3.md) |
| demo_location | 定位 + 逆地理编码 | `liot_location_demo_thread` | `HWDEMO_LOCATION_EN`（+ `HWDEMO_GNSS_EN` / `HWDEMO_LOCATION_HEADLESS_EN`） | [README.md](src/demo_location/README.md) |
| demo_sound_es8375 | ES8375 板载音频通路说明 | 由声音三选一实现提供 | `HWDEMO_SOUND_ES8375_EN` | [demo_sound_es8375.md](src/demo_sound_es8375/docs/demo_sound_es8375.md) |

## 4. 声音三选一

当 `HWDEMO_SOUND_ES8375_EN=y` 时（默认 n，需显式开启），按优先级三选一运行：

| 开关组合 | 运行实现 | 日志前缀 | 说明文档 |
| --- | --- | --- | --- |
| `HWDEMO_MP3_STANDALONE_EN=y` | demo_mp3_standalone | `[mp3-standalone]` | [demo_mp3_standalone.md](src/demo_mp3_standalone/docs/demo_mp3_standalone.md) |
| `HWDEMO_XIAOZHI_EN=y`（需显式开启，config 默认 `n`） | demo_xiaozhi | `[xiaozhi]` | [demo_xiaozhi.md](src/demo_xiaozhi/docs/demo_xiaozhi.md) |
| 以上均 `n` | demo_1khz_tone（含 AT 音量命令） | `[tone-1khz]` | [demo_1khz_tone.md](src/demo_1khz_tone/docs/demo_1khz_tone.md)（含 AT 音量命令章节） |

`HWDEMO_MP3_EN=y`（LVGL MP3 播放器）走独立 `liot_mp3_demo_thread`，不受声音三选一影响；`HWDEMO_SOUND_ES8375_EN` 默认 n，无需显式关闭。

## 5. 单元测试

各 demo 目录下的 `test/` 提供纯逻辑单元测试，触发与运行方式见各 `test/README.md`。
