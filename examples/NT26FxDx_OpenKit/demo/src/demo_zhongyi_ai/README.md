# 中移 AI 语音对话 Demo

## 1. Demo 简介

`demo_zhongyi_ai` 是面向 `L-CT4IT00-YP00W-01_V04` 开发板和 `NT26F6D0` 模组的中移（杭研）AI 平台语音对话示例。

Demo 使用板载 ES8311、麦克风和扬声器完成语音采集与播放，通过移动网络获取平台鉴权信息，并使用安全 WebSocket 与 AI 平台交互。用户按下 `KEY_USER0` 后即可发起一轮语音对话。

## 2. 主要功能

- 等待 SIM 卡注册并建立分组数据连接。
- 通过 HTTP 接口获取 AI 平台 Token。
- 连接固定地址 `wss://demo.hjq.komect.com/speech/ws/`。
- 完成 WebSocket `connection -> hello` 握手及 ping/pong 保活处理。
- 通过 `KEY_USER0`（GPIO20）触发一轮语音交互，并进行按键消抖。
- 使用 ES8311 和板载麦克风采集 16 kHz、单声道 PCM 音频。
- 使用 SDK Opus 库按 60 ms 一帧编码并上传音频。
- 接收云端 VAD 结束事件，停止本轮录音上传。
- 接收平台下发的 Opus 音频，实时解码并通过板载扬声器播放。
- 在接收或播放语音时再次按键，可中断当前交互并开始新一轮录音。
- 输出网络、Token、WSS、录音、VAD、TTS、播放和异常等关键日志。

## 3. 目录结构

```text
demo_zhongyi_ai/
├── inc/                         头文件和本地配置
│   ├── ai_app_private_config.example.h
│   └── ...
├── src/                         Demo 实现
│   ├── user_main.c              程序入口
│   ├── ai_dialog.c              语音对话流程
│   ├── ai_http_token.c          HTTP Token 获取
│   ├── ai_ws_client.c           WebSocket 客户端
│   ├── ai_audio.c               ES8311 录音与播放
│   ├── ai_opus.c                Opus 编解码
│   ├── ai_key.c                 KEY_USER0 按键处理
│   └── ...
└── README.md
```

## 4. 使用前准备

1. 使用 `L-CT4IT00-YP00W-01_V04` 开发板和 `NT26F6D0` 模组。
2. 插入可正常注册和使用数据业务的 SIM 卡，并连接天线。
3. 确认板载麦克风、扬声器和 `KEY_USER0` 可用。
4. 准备 AI 平台分配的 `deviceType`、`deviceId` 和密钥。

## 5. 配置平台参数

复制示例配置：

```powershell
Copy-Item `
  examples/NT26FxDx_OpenKit/demo/src/demo_zhongyi_ai/inc/ai_app_private_config.example.h `
  examples/NT26FxDx_OpenKit/demo/src/demo_zhongyi_ai/inc/ai_app_private_config.h
```

然后编辑 `ai_app_private_config.h`，至少填写：

```c
#define AI_APP_DEVICE_TYPE "平台分配的设备类型"
#define AI_APP_DEVICE_ID "平台分配的设备 ID"
#define AI_APP_SECRET_KEY "平台分配的密钥"
```

如果 SIM 卡需要指定 APN，可同时填写 `AI_APP_APN`、`AI_APP_APN_USER`、`AI_APP_APN_PASSWORD` 和 `AI_APP_APN_AUTH_TYPE`；通常可保持为空，使用模组或 SIM 卡默认 APN。

`ai_app_private_config.h` 已加入 `.gitignore`。请勿提交真实密钥，也不要在日志、截图或问题报告中公开完整密钥和 Token。

## 6. 编译方法

仓库默认配置已经选择：

```makefile
PROJECT=NT26FxDx_OpenKit
BUILD_MODE=demo
DEMO_NAME=demo_zhongyi_ai
```

Windows 下在仓库根目录执行：

```powershell
.\build.bat all
```

Linux 下执行：

```bash
make all
```

成功后生成固件包：

```text
gccout/NT26FxDx_OpenKit/NT26FxDx_OpenKit_NT26F6D0_01.binpkg
```

WebSocket 和 Opus 依赖由 V04 工程配置中的以下开关启用：

```makefile
BUILD_COMP_THIRDPART_EN = y
BUILD_THPART_WEBSOCKET_ENABLE = y
BUILD_THPART_OPUS_ENABLE = y
```

## 7. 烧录与运行

1. 将生成的 `.binpkg` 固件烧录到开发板。
2. 重启开发板，等待网络注册、HTTP Token 获取和 WSS 握手完成。
3. 听到提示音并看到 `ai_dialog idle, press KEY_USER0` 日志后，按一次 `KEY_USER0`。
4. 对板载麦克风说话；云端 VAD 判断说话结束后，设备会停止上传。
5. 等待平台返回语音，设备将通过板载扬声器播放。
6. 在平台语音接收或播放期间再次按下 `KEY_USER0`，可打断当前回答并开始新一轮对话。

单轮录音最长约 15 秒；如果云端 VAD 未提前结束，本地将在达到最长时间后停止录音。

## 8. 串口日志

应用日志默认输出到 `L_USBCOM`，波特率为 115200。在 Windows 上通常显示为 `Lierda Uart Port`。

建议重点关注以下日志：

- `ai_net ready`：网络和数据连接就绪。
- `ai_http token request success`：Token 获取成功。
- `ai_ws hello ack confirmed, connected`：WSS 握手完成。
- `ai_dialog idle, press KEY_USER0`：设备已进入待交互状态。
- `ai_dialog recording`：开始录音上传。
- `ai_dialog VAD end`：云端 VAD 判定结束。
- `ai_dialog tts_start` / `ai_dialog tts_stop`：平台语音开始或结束。
- `ai_dialog interrupted`：用户按键打断当前交互。
- `ai_dialog round complete`：本轮交互完成。

## 9. 可选本地回环测试

如需单独验证 ES8311、麦克风、扬声器和 Opus 编解码链路，可在 `src/user_main.c` 中启用：

```c
#define AI_PHASE_A_LOOPBACK
```

重新编译烧录后，每次按下 `KEY_USER0` 会录制约 2 秒音频，依次进行 Opus 编码、解码并从扬声器播放。完成硬件验证后应注释该宏，恢复默认云端语音对话模式。

## 10. 常见问题

- 长时间停留在网络注册阶段：检查 SIM 卡、天线、信号、APN 和数据业务状态。
- HTTP Token 获取失败：检查设备参数、密钥、系统时间和网络连通性。
- WSS 连接失败：先确认 Token 获取成功，再检查 TLS、域名解析和数据网络。
- 按键无响应：确认使用的是 `KEY_USER0`，其硬件映射为 GPIO20。
- 无法录音或播放：检查 ES8311、麦克风、扬声器及音频连接，并先运行本地回环测试。
- 平台无语音返回：结合 VAD、TTS 和 WebSocket 收发日志判断是上传、协议还是平台响应问题。
