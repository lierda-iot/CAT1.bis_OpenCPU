# 电子吧唧功能简要使用说明

## 1. 功能范围

当前支持的功能：

- LVGL 360 x 360 电子吧唧页面
- 内置图片显示、图片切换、自动播放和删除
- GIF 图片播放
- JPEG 图片下载、校验、解码和显示
- HTTP RGB565 图片下载、校验和显示
- MQTT 设备上线、心跳、绑定、图片任务接收和结果上报
- 图片下载断点续传和本地存储

## 2. 编译

在仓库根目录执行：

```bash
make build PROJECT=L_CT4IT02_1698W \
    BUILD_MODE=demo \
    MODEM=NT26F9D0 \
    MODEMPKG=F9D_A
```

电子吧唧相关配置位于：

```text
examples/L_CT4IT02_1698W/Makefile
examples/L_CT4IT02_1698W/driver/src/lvgl/baji_photo/baji_photo_config.h
```

当前关键配置：

```make
BAJI_PHOTO_ENABLE_GIF_SUPPORT=1
BAJI_PHOTO_ENABLE_JPEG_SUPPORT=1
BAJI_PHOTO_HTTP_RAW_RGB565_VERIFY=1
```

如果启用内置图片显示，需要同时启用内置资源链接：

```bash
make build PROJECT=L_CT4IT02_1698W \
    BUILD_MODE=demo \
    MODEM=NT26F9D0 \
    MODEMPKG=F9D_A \
    BAJI_PHOTO_ENABLE_BUILTIN_DISPLAY=1 \
    BAJI_PHOTO_LINK_BUILTIN_ASSETS=1
```

## 3. 使用流程

1. 启动设备并进入电子吧唧页面。
2. 已绑定设备会启动 MQTT 连接，未绑定设备显示绑定二维码。
3. 通过平台下发图片任务，设备通过 HTTP 下载图片并保存到外部 Flash。
4. 下载完成后进行完整性校验，再加入本地图片列表。
5. 页面可以手动切换、自动播放或删除图片。
6. 设备通过 MQTT 上报下载和显示结果。

## 4. 本地文件

电子吧唧数据默认保存在外部 Flash 的 LittleFS 分区：

```text
/flash/baji/
/flash/baji/photos/
/flash/baji/gifs/
/flash/baji/jpeg/
/flash/baji/index.json
```

相关代码目录：

```text
examples/L_CT4IT02_1698W/driver/src/lvgl/baji_photo/
```

协议细节请参阅：

```text
examples/L_CT4IT02_1698W/driver/src/lvgl/baji_photo/docs/protocols/电子吧唧设备端交互协议说明.md
```
