# watch_ai_app

`watch_ai_app` 是 EC718/LIOT RTOS 上的公司通用 AI 设备框架实验目录。

架构设计参考了开源 `xiaozhi-esp32` 项目的分层应用结构，用 C 语言实现平台无关核心，再通过 718 平台层接入网络、音频、显示、存储和 OTA。协议后端对接的是公司自研 AI 服务（`app_ai_ws`），并非小智开源后端。

当前原则：

- 只在本目录内开发。
- 核心层不直接依赖 LIOT/ESP/具体硬件头文件。
- 718 暂缺或尚未确认的能力先通过 `APP_ERR_NOT_SUPPORTED` 占位。
- 阶段性实现，不在本轮编译。

## 当前内容

已包含四部分框架骨架：

- `framework/core`：错误码、公共类型、事件队列、状态机、核心事件循环。
- `framework/services`：display、network、storage、OTA、tool registry、media、camera 抽象。
- `framework/protocols`：通用协议 ops 和 app_ai_ws WebSocket profile 骨架。
- `framework/ui`：基础 AI UI profile 和 headless profile。
- `framework/platform`：OSAL、board、audio/camera/display/net port 的平台抽象/stub。
- `apps/watch_ai_app`：示例装配入口，只展示框架连接方式，不绑定真实 718 驱动。

## 后续落地

718 没有或尚未接入的能力当前会返回 `APP_ERR_NOT_SUPPORTED`。真实平台代码建议按以下顺序补：

1. `platform/osal/osal_liot.c`
2. WebSocket transport
3. I2S/GX8006/Opus audio port
4. LVGL display driver
5. GC032A camera preview frame blit / QR decode
6. storage/FOTA/tool handlers

详细状态见 `docs/PORTING_STATUS.md`。
