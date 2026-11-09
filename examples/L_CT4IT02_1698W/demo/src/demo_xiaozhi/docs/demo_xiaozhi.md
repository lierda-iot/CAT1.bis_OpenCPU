# L_CT4IT02_1698W XiaoZhi AI 语音示例说明

更新时间：2026-10-09

## 1. 示例文件

- 源码目录：`examples/L_CT4IT02_1698W/demo/src/demo_xiaozhi/src/`
- 入口函数：`liot_sound_es8375_demo_thread(void *argv)`（内部创建 `xiaozhi_main` 任务）
- 核心逻辑：`demo_xiaozhi_core.h`（WS URL 解析 / 哈希等）

## 2. 示例用途

端到端验证小智 AI 语音链路：

- 蜂窝网络拨号（PDP/数据通道）与 DNS
- 设备注册 / OTA 配置 / 激活流程（HTTP）
- WebSocket `wss://api.tenclass.net/xiaozhi/v1/` 加密连接
- 设备 hello / MCP initialize / tools/list 协议握手
- 录音采集 → Opus 编码上行 → STT
- 服务端 TTS（Opus 24kHz）→ 解码 → ES8375 播放
- 断线重连策略：服务端关闭后断开待按键，下次按键再发起连接（LVGL 端口图）

## 3. 当前固定配置

- WS 服务：`wss://api.tenclass.net/xiaozhi/v1/`（OTA 接口下发）
- 音频：上行 16kHz 单声道 Opus，下行 24kHz Opus，frame 60ms
- 任务栈：`xiaozhi_main` 32KB
- 依赖：cJSON、Opus、LVGL、GSENSOR

## 4. 电源与引脚

- 音频：ES8375（I2C0/I2S0 板载通路），GPIO25/27 供电使能
- UI：LVGL 圆屏 + SC7A20H

## 5. 编译开关

- 板级宏：`HWDEMO_SOUND_ES8375_EN=y`（config 默认 n，需显式开启）
- 独占开关：`HWDEMO_XIAOZHI_EN=y`（开启后不编译 tone / mp3_standalone）

## 6. 建议编译方式

```bash
make all PROJECT=L_CT4IT02_1698W MODEM=NT26F9D0 MODEMPKG=F9D_A BUILD_MODE=demo HWDEMO_SOUND_ES8375_EN=y HWDEMO_XIAOZHI_EN=y HWDEMO_MP3_STANDALONE_EN=n
```

注意：需要插入有效 SIM 卡并开通数据业务, 若是定向SIM卡可能需要通过发卡运营商添加白名单，否则访问目标服务器会被运营商拒绝。

## 7. 运行方式

启用后系统启动自动创建任务。串口 115200；开机后，长按屏幕(任意位置)，对麦克风说话，等待 TTS 语音回复。

## 8. 预期结果

### 8.1 预期日志前缀

```text
[xiaozhi]
```

### 8.2 预期关键日志（端到端）

- `network ready`
- `activated; websocket configuration ready`
- `WS upgrade response=HTTP/1.1 101 Switching Protocols`
- `WS device hello ret=0`
- `protocol ready; talk enabled`
- `STT text=...`
- `TTS text=...` / `TTS stream play ...`
- `websocket exited ret=...`（断开）
- `ws idle; connect on next press`（服务端关闭后进入断开待按键状态）
- `websocket re-run ret=...`（按键触发重连）
- `websocket setup failed; reconnect in 5s`（连接建立阶段失败重试）
- `press reconnect attempt=1/3`（按键重连，最多 3 次、间隔 2s）
- `press reconnect closed again; wait for next press`（重连后再次被服务端关闭）
- `ws wait wake failed; retry in 1s`（断开待按键期间 60s 兜底等待超时，安全网，非错误）

### 8.3 实测日志（真机 2026-10-08）

```text
[xiaozhi] data call ret=0x0
[xiaozhi] PDP ret=0x0 cid=1 ipver=1 v4state=1 v6state=0
[xiaozhi] PDP IPv4=31.152.208.181
[xiaozhi] PDP DNS1=114.114.114.114
[xiaozhi] network ready
[xiaozhi] OTA HTTP ret=0 status=200
[xiaozhi] activated; websocket configuration ready
[xiaozhi] WS upgrade response=HTTP/1.1 101 Switching Protocols
[xiaozhi] WS device hello ret=0
[xiaozhi] WS server hello session=... rate=24000 frame_ms=60
[xiaozhi] WS MCP reply method=initialize id=1 ret=0
[xiaozhi] WS MCP reply method=tools/list id=2 ret=0
[xiaozhi] protocol ready; talk enabled
[xiaozhi] STT text=我。
[xiaozhi] TTS text=您好，请问有什么可以帮您的吗？
[xiaozhi] TTS stream play slot=0 bytes=16000
[xiaozhi] STT text=今天天气。
[xiaozhi] TTS text=东莞今天阴天，气温26度，东北风2级，体感温度28度，湿度58%。
[xiaozhi] websocket exited ret=-21
[xiaozhi] ws idle; connect on next press
```

说明：

- 日志覆盖蜂窝数据拨号 → OTA 激活 → WS 101 握手 → hello/MCP 协议 → STT（“我。”、“今天天气。”）→ TTS Opus 24kHz 解码播放 → 服务端断开后进入断开待按键的完整流程。
- 文件末尾出现 `Error reading from serial device` 为串口工具采集结束提示，非设备异常。

### 8.4 验证状态

- **真机端到端已验证**：网络 → WS 101 → hello/MCP → STT（“我。”、“今天天气。”）→ TTS Opus 24kHz 播放 → 服务端关闭后断开待按键
- **Phase 1 轻量优化后真机验证**：
  - 开机 `powerup reason=2`（正常上电），整轮无复位、无 panic
  - 空闲后被服务端关闭 → `ws idle; connect on next press`，不再自动重连、无 SSL churn
  - 按键 → `press reconnect attempt=1/3` → 重连成功 → STT/TTS 对话正常
  - 第二次空闲关闭 → `press reconnect closed again; wait for next press`，直接回待按键
- 纯逻辑（WS URL 解析等）由单元测试覆盖，见 [test/README.md](../test/README.md)。

### 8.5 断线重连策略（Phase 1 轻量优化）

当前行为（2026-10-09 起）：

- 服务端空闲关闭 WS（`ret=-21`）或正常退出（`ret=0`）后，设备进入
  **断开待按键**状态，不再每 5s 自动重连，避免长时间空闲下的 SSL churn；
- 下次按键时触发重连，最多尝试 `XZ_WS_RECONNECT_MAX=3` 次，
  每次间隔 `XZ_WS_RECONNECT_DELAY_MS=2000`ms；
- 连接建立阶段失败（握手/hello 失败等非 `-21/0` 返回值）仍按
  **5s 间隔自动重试**，不受按键限制；
- 断开待按键期间主循环以 60s 有限等待兜底，每轮超时会打印一次
  `ws wait wake failed; retry in 1s` 后重新等待（安全网，不影响按键唤醒）；
- 按键重连成功后再次被服务端关闭，直接回断开待按键，不消耗重连次数。

## 9. 当前边界

- 依赖云端服务可用性与 SIM 数据业务。
- 激活/设备配置走 OTA 接口，首次使用可能需要激活流程。
- 语音体验受网络时延与拾音环境影响。
