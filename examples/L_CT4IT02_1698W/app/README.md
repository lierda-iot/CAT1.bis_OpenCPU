# L_CT4IT02_1698W App 使用说明索引

更新时间：2026-10-10

## 1. 定位

本目录是 `BUILD_MODE=app` 的固件入口与菜单路由：`src/user_main.c` 负责启动与上电诊断，`src/app_menu.c` 按应用开关组装页面与跳转，`inc/app_features.h` 提供 `APP_SINGLE_MODE`。

## 2. 通用构建命令

```bash
make all PROJECT=L_CT4IT02_1698W MODEM=NT26F9D0 MODEMPKG=F9D_A BUILD_MODE=app LIOT_LVGL_UI_WATCH_PROFILE=typical GCC_PATH=/home/forest/x-workspace/celluar/cat1.bis_opencpu_internal/tool
```

- 构建不稳定时在 `make` 前加 `rtk`，其余参数完全相同。
- 开关只写需要改动的部分，示例见第 4 节；`make all` 自带 clean，切换开关后重新完整编译。

## 3. 应用开关

| 开关 | 应用 | 默认 |
| --- | --- | --- |
| `APP_BAJI_EN` | 吧唧 / 相册同步 | `y` |
| `APP_WATCHFACE_EN` | 表盘 | `n` |
| `APP_ATTITUDE_EN` | 体感 | `n` |
| `APP_SALARY_EN` | 计算器 | `n` |
| `APP_MAP_EN` | 地图/定位 | `n` |
| `APP_MP3_EN` | MP3 音乐 | `n` |
| `APP_WATCHFACE_EARTH_2015_EN` | Earth 2015 表盘皮肤（与 `APP_WATCHFACE_EN` 分开配置） | `y` |

默认即「只启用吧唧」，直接进入吧唧页。`APP_WATCHFACE_EARTH_2015_EN` 单独配置，只在 `APP_WATCHFACE_EN=y` 时生效，不参与「至少启用一个应用」判定；`HWDEMO_*`（含 `HWDEMO_XIAOZHI_EN`）默认均为 `n`。

## 4. 编译命令组合示例

每条命令都是可直接拷贝的完整单行命令，均包含第 2 节的全部参数，并只追加与默认值不同的开关；默认关闭的应用不用出现。

```bash
# 1) 默认组合：仅吧唧，不追加任何 APP_*_EN
make all PROJECT=L_CT4IT02_1698W MODEM=NT26F9D0 MODEMPKG=F9D_A BUILD_MODE=app LIOT_LVGL_UI_WATCH_PROFILE=typical GCC_PATH=/home/forest/x-workspace/celluar/cat1.bis_opencpu_internal/tool

# 2) 仅表盘（Earth 2015 皮肤默认开启）
make all PROJECT=L_CT4IT02_1698W MODEM=NT26F9D0 MODEMPKG=F9D_A BUILD_MODE=app LIOT_LVGL_UI_WATCH_PROFILE=typical GCC_PATH=/home/forest/x-workspace/celluar/cat1.bis_opencpu_internal/tool APP_WATCHFACE_EN=y APP_BAJI_EN=n

# 3) 仅表盘，关闭默认开启的 Earth 2015 皮肤（省约 178 KB FLASH）
make all PROJECT=L_CT4IT02_1698W MODEM=NT26F9D0 MODEMPKG=F9D_A BUILD_MODE=app LIOT_LVGL_UI_WATCH_PROFILE=typical GCC_PATH=/home/forest/x-workspace/celluar/cat1.bis_opencpu_internal/tool APP_WATCHFACE_EN=y APP_BAJI_EN=n APP_WATCHFACE_EARTH_2015_EN=n

# 4) 吧唧 + 体感：吧唧默认 y，只追加体感
make all PROJECT=L_CT4IT02_1698W MODEM=NT26F9D0 MODEMPKG=F9D_A BUILD_MODE=app LIOT_LVGL_UI_WATCH_PROFILE=typical GCC_PATH=/home/forest/x-workspace/celluar/cat1.bis_opencpu_internal/tool APP_ATTITUDE_EN=y

# 5) 仅体感单应用：在示例 4 基础上关掉默认开启的吧唧
make all PROJECT=L_CT4IT02_1698W MODEM=NT26F9D0 MODEMPKG=F9D_A BUILD_MODE=app LIOT_LVGL_UI_WATCH_PROFILE=typical GCC_PATH=/home/forest/x-workspace/celluar/cat1.bis_opencpu_internal/tool APP_ATTITUDE_EN=y APP_BAJI_EN=n

# 6) 吧唧 + 体感 + MP3
make all PROJECT=L_CT4IT02_1698W MODEM=NT26F9D0 MODEMPKG=F9D_A BUILD_MODE=app LIOT_LVGL_UI_WATCH_PROFILE=typical GCC_PATH=/home/forest/x-workspace/celluar/cat1.bis_opencpu_internal/tool APP_ATTITUDE_EN=y APP_MP3_EN=y

# 7) 仅地图（自动带起 HWDEMO_LOCATION_EN / HWDEMO_GNSS_EN）
make all PROJECT=L_CT4IT02_1698W MODEM=NT26F9D0 MODEMPKG=F9D_A BUILD_MODE=app LIOT_LVGL_UI_WATCH_PROFILE=typical GCC_PATH=/home/forest/x-workspace/celluar/cat1.bis_opencpu_internal/tool APP_MAP_EN=y APP_BAJI_EN=n

# 8) 仅计算器（自动带起 HWDEMO_SALARY_CALCULATOR_EN / HWDEMO_MP3_EN）
make all PROJECT=L_CT4IT02_1698W MODEM=NT26F9D0 MODEMPKG=F9D_A BUILD_MODE=app LIOT_LVGL_UI_WATCH_PROFILE=typical GCC_PATH=/home/forest/x-workspace/celluar/cat1.bis_opencpu_internal/tool APP_SALARY_EN=y APP_BAJI_EN=n

# 9) 仅 MP3
make all PROJECT=L_CT4IT02_1698W MODEM=NT26F9D0 MODEMPKG=F9D_A BUILD_MODE=app LIOT_LVGL_UI_WATCH_PROFILE=typical GCC_PATH=/home/forest/x-workspace/celluar/cat1.bis_opencpu_internal/tool APP_MP3_EN=y APP_BAJI_EN=n

# 10) 全部应用：吧唧默认已开，只需再开其余五项
make all PROJECT=L_CT4IT02_1698W MODEM=NT26F9D0 MODEMPKG=F9D_A BUILD_MODE=app LIOT_LVGL_UI_WATCH_PROFILE=typical GCC_PATH=/home/forest/x-workspace/celluar/cat1.bis_opencpu_internal/tool APP_WATCHFACE_EN=y APP_ATTITUDE_EN=y APP_SALARY_EN=y APP_MAP_EN=y APP_MP3_EN=y
```

- 只传 `APP_WATCHFACE_EARTH_2015_EN=y` 不会启用表盘：它只选皮肤，须与 `APP_WATCHFACE_EN=y` 同用。
- 全部应用关掉（示例 1 的完整命令再加 `APP_BAJI_EN=n`）会让 `make` 直接报错，见第 5 节。
- 实测 FLASH：示例 6 为 1 930 656 B（39.54%）；示例 10 为 3 239 664 B（66.35%，余量偏紧）。单应用数据见第 6 节。

## 5. 运行行为与联动

- 只启用一个应用时 `APP_SINGLE_MODE=1`（`inc/app_features.h`），上电直接进入该应用。
- 多应用菜单顺序为 TIME → BAJI → ATTUITION → SALARY → POSITIONING → MP3；被关闭的应用不编译。
- MP3 页需同时满足 `APP_MP3_EN=y` 与 `HWDEMO_MP3_EN=y`。
- 自动联动（仅 `BUILD_MODE=app`）：`APP_MAP_EN=y` → `HWDEMO_LOCATION_EN`/`HWDEMO_GNSS_EN`/`BUILD_THPART_CJSON_ENABLE`；`APP_SALARY_EN=y` → `HWDEMO_SALARY_CALCULATOR_EN`/`HWDEMO_MP3_EN`/`BUILD_THPART_MP3_ENABLE`；`APP_MP3_EN=y` → `HWDEMO_MP3_EN`/`BUILD_THPART_MP3_ENABLE`。
- 未启用任何应用时 `make` 直接报错 `BUILD_MODE=app requires at least one APP_*_EN=y`。
- 真机（2026-10-10，全应用组合）按 BAJI → MP3 → POSITIONING → SALARY → ATTUITION → BAJI 逐页进出：各页 `enter`/`exit end` 成对收尾，除 `BOOT_REASON` 上报行外无 `error`/`fail`；MP3 页 `StreamStart ret=0` 后可暂停并退出；定位页室内维持 `fix=0 sats=00`（同 demo_location）。
- 同次 `[baji_mark] BOOT_REASON` 报 `reason=7 name=panic`，与 demo 侧 `powerup reason=2` 不一致，属待确认的启动原因上报现象，排障时保留该行日志。

## 6. 参考体积与相关文档

单应用 FLASH：baji 572 420 B、watchface 544 852 B（`EARTH=n` 时 366 332 B）、attitude 303 276 B、salary 1 232 416 B、map 1 175 788 B、mp3 1 524 532 B。

app 模式的全部开关与编译组合见本文档；demo 模式下各 demo 的编译命令与使用说明见 [demo/README.md](../demo/README.md)。
