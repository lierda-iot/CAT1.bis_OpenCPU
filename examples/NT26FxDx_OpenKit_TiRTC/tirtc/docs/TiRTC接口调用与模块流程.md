<!-- SPDX-FileCopyrightText: 2026 Shenzhen Tange Intelligent Technology Co., Ltd. <https://tange.ai> -->
<!-- SPDX-License-Identifier: Apache-2.0 -->

# TiRTC 接口调用与模块流程

> 适用工程：`NT26FxDx_OpenKit_TiRTC/tirtc`；硬件：NT26F6D0 / F6D_A。
> 核对基线：2026-09-05，源码提交 `3d88ed9`；本地 `tiRTC.h` 版本宏为 2.3.0。  
> 本文解释“应用在哪里调用 SDK、传什么、等什么回调、完成什么功能”，不改动运行流程。

## 1. 阅读范围与文档分工

| 文档 | 主要回答的问题 |
| --- | --- |
| [当前设备操作](当前设备操作.md) | 用户怎样绑定、按键、对讲，OLED 显示什么 |
| [代码与流程详解](代码与流程详解.md) | 工程各文件职责、任务、资源及状态机怎样配合 |
| 本文 | 这些业务最终用到了哪些 TiRTC API；调用方向、参数、回调与释放边界是什么 |

建议先看第 2 章架构，再按“启动 → AI / WX / DEV / LIVE → 退出恢复”阅读；查具体函数可直接看第 14 章完整调用索引。

### 1.1 以哪个版本为准

本文以**本地头文件和实际 C 调用表达式**为主，使用[官方 C API 说明](https://docs.tange.ai/products/tirtc/api-reference/c.html)核对公开契约。在线文档中的其他平台、新版本示例或仅出现在注释里的函数，不代表当前固件已经调用。

`sdk/include/tirtc` 放的是声明、类型和说明，不是完整 SDK 实现。真正的连接协商、传输等实现链接自 `sdk/lib/libTiRTC.a`。本文不会把“头文件声明了某接口”写成“应用已经接入”，也不凭声明推断静态库内部线程和释放细节。

### 1.2 四个头文件的分工

| 头文件 | 内容 | 当前应用使用情况 |
| --- | --- | --- |
| [tiRTC.h](../sdk/include/tirtc/tiRTC.h) | 生命周期、配置、连接、音视频、命令、订阅、日志、WHIP、服务请求；30 个函数声明 | 直接调用其中 17 个函数 |
| [tgtrp.h](../sdk/include/tirtc/tgtrp.h) | 下层 listener / connection / channel、媒体传输、WHIP、统计及日志；42 个函数声明 | 只直接调用 2 个日志函数 |
| [tiRTC_stat.h](../sdk/include/tirtc/tiRTC_stat.h) | 耗时统计、视频发送监控、连接方式选择标志；3 个函数声明 | 当前业务没有调用 |
| [basedef.h](../sdk/include/tirtc/basedef.h) | 基础类型、导出宏、编译器与结构体打包定义 | 由 `tiRTC.h` 引入；没有可调用函数 |

因此本工程直接调用此目录声明的 **19 种 SDK 函数**。统计范围为应用源码，不包括头文件里的示例、宏定义展开示意或库内部调用。

## 2. 整体架构：三条路径不要混淆

```mermaid
flowchart TB
    UI["三按键 / OLED<br/>ui_controller.c"]
    FEATURES["业务模块<br/>AI：ai_chat.c；WX：wechat_call.c<br/>DEV：device_call.c；LIVE：platform_intercom.c"]
    BIZ["业务平台接入<br/>device_binding.c：绑定 / token / HTTP<br/>formal_mqtt.c：来电 / 房间信令"]
    RT["唯一 TiRTC runtime<br/>tirtc_runtime.c<br/>owner / generation / 连接回收"]
    AUDIO["共享音频 / Opus / G.711<br/>audio_device.c 等"]
    SDK["TiRTC SDK<br/>tiRTC.h + libTiRTC.a"]
    CLOUD["业务平台<br/>绑定、联系人、呼叫房间"]
    PEER["AI 服务 / 微信服务 / 对端设备 / 网页"]
    HW["利尔达音频驱动<br/>ES8311 / I2S"]
    UI --> FEATURES
    FEATURES <-->|"业务请求 / 信令事件"| BIZ
    BIZ <-->|"liot_httpc_* / MQTT"| CLOUD
    FEATURES <-->|"demo_tirtc_* / listener"| RT
    RT <-->|"TiRtc* / 回调"| SDK
    SDK <-->|"连接、命令、编码音频"| PEER
    FEATURES -.->|"WX 拒接直调 TiRtcServiceRequest"| SDK
    FEATURES <-->|"PCM / 编解码"| AUDIO
    AUDIO <--> HW
```

### 2.1 业务平台路径

服务发现、六位码绑定、AI token、WX 联系人、DEV 房间等，主要经过 `device_binding.c` 的 HTTP 封装；来电等信令通过正式 MQTT 接收。

这些不是 `TiRtcConnect()` 自动替应用完成的业务。`network_manager.c` 的注册、PDP、时间，及绑定的 NVM 存储，也不是这四个 TiRTC 头文件里的能力。

### 2.2 实时连接路径

AI / WX / DEV / LIVE 不各自 `TiRtcInit()`。它们共享 `tirtc_runtime.c` 启动的一个设备级 runtime，通过 `demo_tirtc_*` 管理连接、命令和音频。

唯一重要的功能性直调例外是 WX 的 `TiRtcServiceRequest()`；此外业务文件会直接调用 `TiRtcGetErrorStr()` 打印错误。

### 2.3 声音硬件路径

TiRTC 发送或接收**已经编码的音频数据**，不替这块板子配置 ES8311、录音、Opus/G.711 编解码或播放 PCM。麦克风和扬声器由本地音频层驱动。

`SET` 页面只调整音量和麦克风级别，没有自己的 TiRTC 连接，也不调用这四个头文件里的音量 API——这里没有这种音量 API。

## 3. 模块一：开机、联网绑定与 SDK 启动

主要文件：[app_entry.c](../src/app/app_entry.c)、[network_manager.c](../src/services/network_manager.c)、[device_binding.c](../src/services/device_binding.c)、[tirtc_runtime.c](../src/core/tirtc_runtime.c#L1998)。

### 3.1 启动前先准备什么

`user_main()` 创建业务、网络、绑定和 runtime 等任务。任务创建不等于功能就绪：WX / DEV 可以先建立信令接收入口，再等待绑定和 TiRTC ready；LIVE 的接收路由也要先注册，避免 SDK 启动后入站连接没有接收者。

`demo_tirtc_task()` → `runtime_start_until_terminal()` 等待 `demo_binding_get_tirtc_identity()` 提供身份，然后调用 `runtime_start()`。身份快照的用途如下：

| 数据 | 去向 | 不要混淆 |
| --- | --- | --- |
| `device_id` | `TiRtcStart(device_id, &s_callbacks)` | 设备身份，不是 AI/WX 的 service descriptor |
| `device_key` | `TIRTC_OPT_DEVICE_SECRET_KEY` | 长期设备密钥，不是某次呼叫的 token |
| `client_id` | `TIRTC_OPT_CLIENT_ID` | 稳定的硬件/生产标识 |
| `iccid` | `TIRTC_OPT_ICCID` | 当前 4G SIM 信息，不是对端设备 ID |
| `tirtc_endpoint` | 条件设置 `TIRTC_OPT_SERVICE_ENDPOINT` | TiRTC 服务入口，不等同于所有业务 HTTP 的 base URL |

### 3.2 实际启动时序

```mermaid
sequenceDiagram
    participant B as 网络 / Binding
    participant R as Runtime 任务
    participant S as TiRTC SDK
    participant F as AI / WX / DEV / LIVE
    B-->>R: identity 可用
    R->>S: 设置 TiRTC 与 TGTRP 日志
    R->>S: SetOption(MAX_SEND_BUFFER, 131072)
    R->>S: TiRtcInit()
    R->>S: 设置连接数、4G、ICCID、身份等
    alt Init 后配置失败
        R->>S: TiRtcUninit()，报告启动错误
    else 配置成功
        R->>S: TiRtcStart(device_id, 静态 callbacks)
        alt Start 立即返回错误
            R->>S: TiRtcUninit()，按错误类型处理
        else Start 返回 0
            S-->>R: on_event(SYS_STARTED)
            R-->>F: runtime ready，可申请会话
            Note over R,S: 未等到事件则进入软件恢复，不把返回 0 当作已启动
        end
    end
```

图省略了 Init 本身失败等立即返回路径；这些路径不会继续调用 Start。正常运行期间 runtime 常驻，退出一个业务不会重新初始化整个 SDK。启动立即失败且错误可重试时，会在约 5 秒后重试启动；已经受理 Start 的生命周期异常则走第 11 章软件恢复。

### 3.3 当前显式设置的参数

配置来源：[项目 config](../../config)，由 [tirtc_app.mk](../tirtc_app.mk) 传入编译。以下是当前默认配置；命令行覆盖构建变量时应重新核对。

| 顺序 / 选项 | 传入内容 | 调用位置 `tirtc_runtime.c` | 功能 |
| --- | --- | --- | --- |
| `TiRtcLogSetCallback` | `tirtc_sdk_log_callback` | 2017 | 接管高层日志输出 |
| `tgtrp_set_log_callback` | `tirtc_transport_log_callback` | 2018 | 接管传输层日志输出 |
| `TiRtcLogSetLevel` | 15 | 2019 | 当前诊断日志级别；不是源码 fallback 的 3 |
| `tgtrp_set_log_level` | 3；统计位默认关闭 | 2020 | 控制传输层日志 |
| `MAX_SEND_BUFFER` | `uint32_t`，131072 B＝128 KiB | 2025 → 1988 | **在 Init 前**设置发送缓冲上限；不是当前实际已用量 |
| `MAX_CONNECTIONS` | `int`，1 | 2055 → 1988 | 一次仅允许一条媒体连接 |
| `NETWORK_TYPE` | `int`，`TIRTC_NETCONN_4G` | 2059 → 1988 | 明确本机通过 4G 联网 |
| `ICCID` | `identity->iccid` | 2064 → 1988 | 4G SIM 信息 |
| `RESTRICTED_NETWORK` | `int`，1 | 2069 → 1988 | 当前工程按受限网络配置；不是由所有 4G 自动推导 |
| `SERVICE_ENDPOINT` | 自定义入口字符串 | 2078 → 1988 | 只有非默认入口才显式设置 |
| `DEVICE_SECRET_KEY` | `identity->device_key` | 2085 → 1988 | 设备模式身份配置 |
| `CLIENT_ID` | `identity->client_id` | 2092 → 1988 | 稳定生产标识 |

表内省略了选项统一前缀 `TIRTC_OPT_`。`runtime_set_option()` 才是实际调用 `TiRtcSetOption()` 的地方。数值传地址和 `sizeof`；身份字符串传指针和 `strlen`，不把末尾 NUL 算入长度。

`runtime_is_default_endpoint()` 将空入口及内置默认域名视为“不覆盖 SDK 默认值”。当前没有显式设置 `TIRTC_OPT_WAKEUP`、`TIRTC_OPT_CONNECT_CACHE`、`TIRTC_OPT_APP_ID`、`TIRTC_OPT_TGTRP_POLL_TIMEOUT`，不能把 SDK 缺省值写成应用已经设置的行为。

启动完成依据是 `TIRTC_EVENT_SYS_STARTED`，不是 Start 返回 0；这与[官方连接流程](https://docs.tange.ai/products/tirtc/guides/connection.html)一致。当前启动事件等待上限为 30 秒；超时走第 11 章恢复流程。

## 4. 模块二：公共 Runtime 封装

主要文件：[tirtc_runtime.h](../include/tirtc_runtime.h#L120)、[tirtc_runtime.c](../src/core/tirtc_runtime.c#L2209)。

### 4.1 业务为什么不直接拿句柄调用 SDK

连接句柄 `tirtc_conn_t` 是 SDK 的不透明对象，不是应用可以 `free()` 的内存。本项目在 SDK 外再记录 owner、generation、引用计数和正在关闭的连接，防止上一通的清理影响下一通。

| 应用概念 | 本项目含义 |
| --- | --- |
| `owner` | AI、WECHAT、DEV_CHAT、LIVE 中哪一个业务占用共享连接 |
| `session_generation` | 一次业务占用的代次，申请 owner 时产生 |
| `request_generation` | 一次异步建连尝试的代次 |
| `connection_generation` | 连接状态更新时使用的代次 |
| `active` | 当前允许收发的句柄 |
| `closing` | 已停止业务访问，但仍待 SDK 释放确认的句柄 |
| `users` | 尚未退出的受管理句柄使用者数量 |
| pending / reservation | 尚未完成的连接请求、回调或回收记录；不能因 UI 已返回就清空 |

这些 generation 是应用实现的保护，不是 SDK 自动附带到每个回调中的连接 ID。它们需要配合固定 context、句柄匹配及真实释放边界一起工作。

### 4.2 封装到 SDK 的对应关系

| 业务调用 | 最终 SDK 调用 | 封装额外做什么 |
| --- | --- | --- |
| `demo_tirtc_connect()` | `TiRtcConnect()` | 检查 owner/代次，预留 pending，复制 remote ID/token，注册中间结果回调 |
| `demo_tirtc_whip_connect()` | `TiRtcWhipConnect()` | 同上；此路径要求非空服务描述和 token |
| `demo_tirtc_send_command()` | `TiRtcSendCommand()` | 校验 owner/代次，临时持有 active 引用 |
| `demo_tirtc_send_audio()` | `TiRtcSendAudioStream()` | 校验帧和数据，再保护 active 引用 |
| `demo_tirtc_subscribe_audio()` | `TiRtcSubscribeAudio()` | 向对端申请接收指定音频流 |
| `demo_tirtc_get_send_buffer_used()` | `TiRtcGetSendBufferUsed()` | 安全查询，通过输出参数交回已用字节数 |
| `demo_tirtc_disconnect()` | 稍后由 runtime task 调 `TiRtcDisconnect()` | 按 owner/代次撤销本会话等待，或原子地将 active 转为 closing；不改变全局 ready，不是同步释放 |

下列名称**没有同名 SDK 接口**，只操作本地协调状态：

| 应用函数 | 作用 |
| --- | --- |
| `demo_tirtc_session_claim()` / `demo_tirtc_session_release()` | 申请、归还共享媒体 owner |
| `demo_tirtc_expect_incoming()` / `demo_tirtc_cancel_expected_incoming()` | 为 DEV 主叫登记、取消预期的反向连接 |
| `demo_tirtc_adopt_incoming()` | LIVE 认领 SDK 已经交付的入站句柄；不再创建连接 |
| `demo_tirtc_set_listener()` / `demo_tirtc_set_feature_listener()` | 注册 AI / WX / DEV 的业务回调路由 |
| `demo_tirtc_set_incoming_listener()` | 注册 LIVE 被动连接路由 |
| `demo_tirtc_admission_try_enter()` / `demo_tirtc_admission_leave()` | 把 HOME 准入检查和本地占用更新组成一个短事务 |
| `demo_tirtc_wait_ready()` / `demo_tirtc_wait_connections_idle()` | 等待应用维护的就绪或资源排空条件 |
| `demo_tirtc_require_restart()` | 通知 runtime task 请求 SDK 软件恢复，不执行整机复位 |

### 4.3 主动连接如何回到业务模块

```mermaid
sequenceDiagram
    participant F as Feature worker
    participant R as Runtime 封装
    participant S as TiRTC SDK
    F->>R: claim(owner)，取得 session_generation
    F->>R: connect / whip_connect(remote, token, callback)
    R->>R: 先登记 pending，复制 remote/token
    R->>R: 保存 callback 和 user_data 指针
    R->>S: TiRtcConnect / TiRtcWhipConnect
    S-->>R: 返回 0，表示受理
    R-->>F: 返回提交结果，不等于已连通
    S-->>R: runtime_managed_connect_result(error, hconn)
    alt 成功且仍属于本次有效请求
        R->>R: 安装 active
        R-->>F: 业务结果 callback(0, hconn)
    else 已取消、过期或出错
        R->>R: 不安装 active；晚成功句柄登记待关闭
        R-->>F: 业务结果 callback(error, NULL)
    end
    R->>R: callback 返回后再清 pending context
```

这张图表达逻辑完成关系，不保证结果回调一定晚于提交函数返回。代码先登记 pending，正是为了应对快速回调。Runtime 不深拷贝 `user_data` 指向的业务 context，调用方必须保持它有效直到结果 callback 完成。上层的等待期限也只能在提交 API 返回后执行，不能替代底层库自身的超时保证。

## 5. SDK 回调：从库回到哪个模块

静态注册表是 [s_callbacks](../src/core/tirtc_runtime.c#L1968)，整个 SDK 生命周期都有效。`TiRtcStart()` 得到的是这张表，不是 AI/WX/DEV 各自的 listener。

### 5.1 全局回调表

| SDK 回调 | Runtime 入口 | 最终处理 |
| --- | --- | --- |
| `on_event` | `runtime_on_event()` | 维护 started/ready/stopped/restart 状态，不转给业务 listener |
| `on_conn_accepted` | `runtime_on_conn_accepted()` | 预期 DEV 反向连接，或无人认领时尝试 LIVE；拒收的句柄交异步回收 |
| `on_conn_error` | `runtime_on_conn_error()` | 记录错误与待回收状态，通知相应业务停止使用连接 |
| `on_disconnected` | `runtime_on_disconnected()` | 普通连接通知业务并清 closing；拒绝队列的句柄只清拒绝槽，不通知业务 |
| `on_audio` | `runtime_on_audio()` | 转给 `ai_on_audio` / `wx_on_audio` / `dev_on_audio` / `live_on_audio` |
| `on_command` | `runtime_on_command()` | 转给对应模块的 `*_on_command` |
| `on_subscribe_audio` | `runtime_on_subscribe_audio()` | 对端请求设备上行；返回是否接受 |
| `on_unsubscribe_audio` | `runtime_on_unsubscribe_audio()` | 对端停止订阅请求；具体是否关闭采集由各业务决定 |
| `on_subscribe_video` / `on_unsubscribe_video` | 同名 `runtime_on_*` | 仅转给 LIVE incoming listener，处理兼容性请求 |
| `on_video` | `runtime_on_video()` | 空实现，不播放视频 |
| `on_message` | `runtime_on_message()` | 空实现；不是 `on_command` 的替代入口 |
| `on_request_key_frame` | `runtime_on_request_key_frame()` | 空实现，没有视频编码器 |
| `on_update_bitrate` | `runtime_on_update_bitrate()` | 空实现，没有视频码率自适应业务 |

连接结果是另一类回调：`TIRTCCONNECTCALLBACK`。它通过 `TiRtcConnect/WhipConnect` 的参数注册，**不在 `TIRTCCALLBACKS` 结构里**。

| 业务 | Listener 定义 / 注册位置 | 主动连接结果链路 |
| --- | --- | --- |
| AI | `ai_chat.c:778 / 1907` | SDK → `runtime_managed_connect_result` → `ai_whip_connect_callback`（787） |
| WX | `wechat_call.c:963 / 2313` | SDK → 同一中间回调 → `wx_whip_connect_callback`（992） |
| DEV | `device_call.c:1425 / 3444` | SDK → 同一中间回调 → `dev_connect_callback`（1472） |
| LIVE | `platform_intercom.c:673 / 1233` | 没有主动 connect callback，走 `live_on_conn_accepted`（318） |

### 5.2 分发不是无条件广播，也不是永远单播

通常 active 句柄、owner 和 generation 匹配时，音频、命令等只转给当前 owner，并保护引用计数。

未匹配 managed 路径时，代码仍保留兼容分发：先 LIVE incoming listener，再遍历 feature listener；各模块必须自行检查句柄/会话。音频订阅会尝试全部有效接收者，任一个返回 0 即视为接受。

入站连接的规则不同：已有 owner 时只能按其预期条件接收；无 owner 时按 AI → WX → DEV 尝试，首个接受即停止，最后才尝试 LIVE。当前 AI/WX 没实现入站接受回调。`on_conn_error(NULL, …)` 直接忽略；拒绝句柄的 `on_disconnected` 只完成拒绝回收，不再分发。不要把所有回调都画成同一种路由。

### 5.3 回调内的数据和线程边界

- SDK 回调中的 audio / command payload 在回调返回后不再有效。Runtime 同步转发，不替业务复制。
- 需要后续处理的数据，由 feature 复制到固定 pool，再把 slot index 投入队列，交 worker 处理。
- callback 不执行耗时 HTTP、录音、播放清理、`TiRtcStop/Uninit` 或整机 reset。
- `on_conn_error` 不是已释放通知；应先停止业务收发，再由 worker 提交断开。
- 音频订阅回调是同步决定接受/拒绝的入口，要迅速返回，不能在那里长时间初始化硬件。

## 6. 模块三：AI 对讲

主要文件：[ai_chat.c](../src/features/ai_chat.c)，业务入口 `ai_run_session()`。

### 6.1 建连、协商、媒体时序

```mermaid
sequenceDiagram
    participant U as UI
    participant A as AI worker
    participant B as Binding HTTP
    participant R as Runtime / TiRTC
    participant C as AI 服务
    U->>A: 进入 AI，提交开始请求
    A->>R: wait_ready
    A->>B: 取得有效 AI access；预取可用则消费预取
    B-->>A: peer_id / token / role 等
    A->>R: claim AI，whip_connect(peer_id, token)
    R->>C: TiRtcWhipConnect
    R-->>A: ai_whip_connect_callback 成功
    A->>A: 约 300 ms settle 窗口内准备音频和 Opus
    A->>R: send_command(0x2100, start_session JSON)
    R->>C: TiRtcSendCommand
    C-->>R: 0x2100，匹配 id 的协商响应
    R-->>A: ai_on_command → 队列 → ai_handle_command
    A->>R: subscribe_audio(1)
    loop 半双工对话
        A->>R: LISTENING：发送 stream1 Opus
        R->>C: TiRtcSendAudioStream
        C-->>R: AI 回复音频及轮次事件
        R-->>A: on_audio / on_command
        A->>A: AI SPEAK：解码播放，暂停录音
    end
```

AI access 的获取是业务 HTTP，不是 `TiRtcServiceRequest()`。进入会话时可以消费 HOME 预取的短期、单次凭据；过期或不可用时走原有 fresh request。

### 6.2 函数调用索引

| 阶段 | `ai_chat.c` 中的函数 / 调用行 | 封装 → SDK / 功能 |
| --- | --- | --- |
| 等待 runtime | `ai_run_session`，1590 | `demo_tirtc_wait_ready`，不重复 Init/Start |
| 申请 owner | 同上，1625 | `demo_tirtc_session_claim(AI)` |
| 主动连接 AI 服务 | `ai_whip_submit`，890 | `demo_tirtc_whip_connect` → `TiRtcWhipConnect` |
| 启动应用会话 | `ai_send_start_session`，1088 | `demo_tirtc_send_command` → `TiRtcSendCommand` |
| 订阅回复音频 | `ai_run_session`，1741 | `demo_tirtc_subscribe_audio(1)` → `TiRtcSubscribeAudio` |
| 检查发送积压 | `ai_send_uplink_20ms`，1226 | `demo_tirtc_get_send_buffer_used` → `TiRtcGetSendBufferUsed` |
| 发送麦克风音频 | 同上，1260 | `demo_tirtc_send_audio` → `TiRtcSendAudioStream` |
| 通知服务结束 | `ai_send_end_session`，1386 | 相同 `0x2100` 通道发送 `end_session` JSON |
| 关闭连接 / 取消 pending | `ai_disconnect_and_wait`，1419；`ai_session_cleanup`，1509 | `demo_tirtc_disconnect`，最终由 runtime 调 SDK Disconnect |
| 归还 owner | `ai_release_tirtc_session`，568 | `demo_tirtc_session_release` |

### 6.3 AI 不是“连接成功就开始播放”

1. WHIP callback 成功，只说明实时连接已建立。
2. 应用还要发送 `start_session`，等待匹配 JSON-RPC `id` 的响应。
3. 响应需要非空 `session_id`；若包含 input/output 音频格式，必须匹配 Opus / 16000 Hz / 单声道。
4. 协商成功后才订阅 stream1，进入对话循环。

AI 发送和接收都使用**完整命令字 `0x2100`**，不是“低 16 位等于即可”；`ai_on_command()` 和 `ai_handle_command()` 都有完整值检查。应用请求/响应靠 JSON `id` 关联，没有使用 SDK 的 `atomic_get_cmd_sn()`。

`round_start`、`round_end`、`interrupt`、`end_session` 是 JSON 内的业务方法，不是新的 TiRtc API。`round_end` 允许剩余音频播完；`interrupt` 会 Stop 音频、清队列和重置解码状态，但当前底层播放恢复仍有已知限制，不能据此声称下一轮一定正常。

### 6.4 半双工和退出

AI 上行是 20 ms / 320 个 16 kHz PCM sample，经 Opus 编码发送。服务正在说话或本地播放尚未完成时，代码不开始录音。发送缓存**大于** 12 KiB 时跳过本轮采集；发送返回 `TIRTC_E_BUSY` 时丢当前帧。

退出的实际主线：Stop 本地音频 → 条件允许时尝试发送 `end_session` → 关闭应用接收准入、等待 producer 并 drain 队列 → 提交 managed disconnect → 归还音频和 TiRTC owner。未完成的 WHIP 即使尚未给业务公开句柄，也要取消本地请求并保留最终回调的回收责任。

## 7. 模块四：微信 WX 对讲

微信端使用“探鸽小钛”小程序；设备端仍以 `WX` 菜单和 `wechat_call.c` 模块承载该功能。

主要文件：[wechat_call.c](../src/features/wechat_call.c)。

### 7.1 主叫与被叫共用哪段流程

```mermaid
flowchart TD
    O["设备选择联系人 / KEY1"] --> H["Binding HTTP<br/>POST /v1/voip/device/call"]
    H --> M["MQTT 收到匹配的 call_incoming"]
    I["微信主动呼叫设备"] --> G{"MQTT call_incoming<br/>HOME / 空闲 / 准入允许？"}
    G -->|"否：忙拒绝"| J["wx_reject<br/>TiRtcServiceRequest"]
    G -->|"是"| R["进入 RINGING<br/>显示 INCOMING"]
    R --> K{"本地处理"}
    K -->|"KEY1 接听"| W["wx_start_whip<br/>claim WECHAT"]
    K -->|"拒接 / 超时"| J
    M --> W
    W --> C["demo_tirtc_whip_connect<br/>TiRtcWhipConnect"]
    C --> CB["wx_whip_connect_callback 成功"]
    CB --> S["WAIT_START<br/>等待服务端 0x2000"]
    S --> A["wx_start_audio<br/>初始化音频，订阅 stream0"]
    A --> T["WX TALK<br/>双向 A-law"]
```

两种角色都由设备主动发起 WHIP，到达 WHIP 成功回调后都要等服务端 `0x2000`。本地 KEY1 接听触发的是 WHIP，不是设备发送一个 `0x2000` 作为接听指令。

### 7.2 函数调用索引

| 阶段 | `wechat_call.c` 中的函数 / 调用行 | 封装 → SDK / 功能 |
| --- | --- | --- |
| 同步 / 呼叫业务 | `wx_fetch_contacts` 1102、profile 请求1191、`wx_start_outgoing_call` 1785 | Binding HTTP；不直接调用 TiRtc 连接 API |
| 主动建立实时连接 | `wx_start_whip`，1270 | `demo_tirtc_whip_connect` → `TiRtcWhipConnect` |
| 订阅下行 | `wx_start_audio`，1500 | `demo_tirtc_subscribe_audio(0)` → `TiRtcSubscribeAudio` |
| 发送上行 | `wx_send_uplink`，1683 | `demo_tirtc_send_audio` → `TiRtcSendAudioStream` |
| 已连接时通知挂断 | `wx_cleanup_session`，1407 | `demo_tirtc_send_command(0x2001, reason JSON)` → `TiRtcSendCommand` |
| 关闭 / 取消连接 | 同上，1420；延迟回收路径616 | `demo_tirtc_disconnect` |
| 归还 owner | `wx_release_tirtc_session`，654 | `demo_tirtc_session_release` |
| 无媒体连接的拒接等 | `wx_reject`，1069 | **直接 `TiRtcServiceRequest`** |

### 7.3 `TiRtcServiceRequest` 在哪里、为什么调用

[wx_reject()](../src/features/wechat_call.c#L1039) 构造 JSON 后调用：

```c
TiRtcServiceRequest(WX_REJECT_PATH, json, NULL,
                    wx_service_response, NULL);
```

| 参数 | 当前值 / 含义 |
| --- | --- |
| `path` | `/v1/wxvoip/reject` |
| `json_body` | `wx_app_id`、`wx_model_id`、`wx_session_token`、`wx_room_id`、`wx_payload`、`hangup_reason` |
| `token` | `NULL`，使用设备身份访问；不是 WX/AI 建连 token |
| `cb` | `wx_service_response`，当前仅记录收到响应，不解析业务成功状态 |
| `user_data` | `NULL` |

用途不局限于手动拒接，还包括忙、超时、异常以及尚未建立媒体连接时关闭相关信令房间。若连接已建立，清理路径可先发送 `0x2001` 再 disconnect；不能用关闭 TiRTC 连接代替所有业务房间关闭操作。

此调用使用 SDK 服务请求路径，**不经过** `device_binding.c` 的 `liot_httpc_*` singleton/reaper。排查问题时，两套 HTTP 生命周期不能混为一谈。入口长度等保护仍受当前 SDK 和应用校验约束。

### 7.4 媒体与命令

- 上下行使用 stream0。设备上行：16 kHz PCM 两点平均降为 8 kHz，再编码 A-law，160 B / 20 ms。
- 下行只接受 A-law，兼容 8 kHz / 16 kHz；8 kHz 解码后重复采样到板端 16 kHz。
- `wx_command_is()` 兼容完整命令、低 16 位及去相应标志后的低 15 位；不要把此兼容规则套用到 AI 或 DEV。
- `on_unsubscribe_audio` 当前为空操作；没有调用 `TiRtcUnsubscribeAudio()`。
- WX 为同时收发，无已验证 AEC；编解码与播放仍由本地音频层完成。

## 8. 模块五：DEV 设备互呼

主要文件：[device_call.c](../src/features/device_call.c)。

### 8.1 最重要的角色区别

**业务主叫设备是这次 P2P 的被动接受方；业务被叫设备在接听后主动调用 `TiRtcConnect()` 连回主叫。** “谁先按呼叫”与“谁调用 P2P Connect”不是同一个概念。

```mermaid
sequenceDiagram
    participant A as 主叫设备 A
    participant C as 呼叫平台 / MQTT
    participant B as 被叫设备 B
    A->>A: claim DEV，进入 DIALING
    A->>A: expect_incoming，先预留反向连接
    A->>C: POST /v1/call/request
    C-->>A: 返回 room_id
    C-->>B: incoming 信令
    B->>B: 准入时 claim DEV，显示 RINGING
    Note over B: 用户按 KEY1 接听
    B->>C: POST /v1/call/device/info
    C-->>B: 主叫 device_id 对应的连接 token
    B->>A: demo_tirtc_connect → TiRtcConnect
    A-->>A: on_conn_accepted → dev_on_conn_accepted
    B-->>B: dev_connect_callback 成功
    B->>A: TiRtcSendCommand(0x2000, room_id JSON)
    B->>B: 订阅 stream10，启动音频
    A->>A: 连接、room_id、确认房间一致后启动音频
    A->>B: stream10 A-law
    B->>A: stream10 A-law
```

图按正常业务关系排列；网络回调可能交错到达。主叫端特意在呼叫 HTTP **之前**登记 `expect_incoming`，并在激活音频前核对房间，不能把这两步移到“全部 HTTP 已完成之后”才处理。

### 8.2 函数调用索引

| 阶段 | `device_call.c` 中的函数 / 调用行 | 封装 → SDK / 功能 |
| --- | --- | --- |
| 主叫占用 owner | `dev_start_outgoing_call`，2615 → `dev_tirtc_claim` 368 | `demo_tirtc_session_claim(DEV_CHAT)` |
| 主叫等待反向 P2P | 同上，2638 → `dev_tirtc_expect_current` 400 | `demo_tirtc_expect_incoming`；这里不调用 `TiRtcConnect` |
| 创建呼叫 | 同上，2670 | Binding HTTP `/v1/call/request` |
| 被叫接听获取凭据 | `dev_accept_incoming`，2803 | Binding HTTP `/v1/call/device/info` |
| 被叫主动 P2P | `dev_submit_callee_connect`，1583 | `demo_tirtc_connect` → `TiRtcConnect` |
| 被叫发房间确认 | `dev_process_signals`，3131 | `demo_tirtc_send_command(0x2000, room_id JSON)` |
| 订阅下行 | `dev_start_audio`，1851 | `demo_tirtc_subscribe_audio(10)` → `TiRtcSubscribeAudio` |
| 发送麦克风数据 | `dev_send_uplink`，2047 | `demo_tirtc_send_audio` → `TiRtcSendAudioStream` |
| 通知对端挂断 | `dev_finish_session`，1747 | 条件允许时发送 `0x2001` / `{"reason":0}` |
| 撤销等待 / 断开 | `dev_tirtc_begin_release`，434、438 | cancel expected → `demo_tirtc_disconnect` |
| 归还 owner | `dev_tirtc_try_release`，459 | `demo_tirtc_session_release` |

### 8.3 房间信令和媒体连接分别关闭

DEV 的 `/v1/call/reject`、`/v1/call/cancel`、`/v1/call/hangup` 通过后台 room-action HTTP worker 执行；它们不使用 WX 的 `TiRtcServiceRequest()`。

房间结束不表示 P2P 内存已经释放；`TiRtcDisconnect()` 提交成功也不表示服务器房间已经关闭。因此 `dev_finish_session()` 根据角色和结束原因分别处理 HTTP 房间动作、可选的对端挂断命令、本地音频和 managed connection。

### 8.4 数据格式与超时

- 命令严格匹配完整值：确认 `0x2000`、挂断 `0x2001`，不像 WX 有位掩码兼容。
- 音频上下行均 stream10、A-law、8 kHz 单声道；上行 160 B / 20 ms。
- 下行 callback 最多接受 640 B 合并包，不保证“一次回调就是一帧 20 ms”；它明确拒绝 16 kHz flags。
- 被叫当前只提交一次主动连接尝试，应用等待 P2P callback 约 15 秒。超时结束本次业务，不整机复位；迟到连接由 runtime 保留的 context 安全接回并关闭。
- 主叫清理保留现有 10 秒迟到入站保护，避免刚取消的反向连接被认成其他功能。
- video 类型房间在本固件仍按 audio-only 处理，不代表调用了视频 SDK API。

## 9. 模块六：主页远端 LIVE

主要文件：[platform_intercom.c](../src/features/platform_intercom.c)，准入来源：[ui_controller.c](../src/ui/ui_controller.c#L269)。

### 9.1 LIVE 是接管连接，不是设备主动外呼

```mermaid
flowchart TD
    P["平台发起连接"] --> S["SDK on_conn_accepted"]
    S --> R["Runtime 优先处理已有 owner / 预期连接"]
    R -->|"无人认领时尝试 LIVE"| L["live_on_conn_accepted"]
    L --> G{"启动 READY、HOME<br/>其他业务 idle、LIVE 空闲？"}
    G -->|"否"| X["拒绝接管<br/>Runtime 回调外断开"]
    G -->|"是"| A["demo_tirtc_adopt_incoming<br/>认领已有 hconn 和 generation"]
    A --> W["唤醒 LIVE worker<br/>准备共享音频"]
    W --> U["订阅下行 stream10 和 stream14<br/>至少一路订阅提交成功"]
    U --> Q["发送 REQ_AUDIO 0x1106<br/>上行默认开启"]
    Q --> T["发送 stream10<br/>接收 stream10 / 14"]
    T --> E["离开 HOME / 远端挂断 / 异常<br/>关闭音频及 managed connection"]
```

这里只描述走到 LIVE 的连接。DEV 已登记反向 P2P 时优先归 DEV；已有业务占用时不能让 LIVE 抢占。

### 9.2 函数调用索引

| 阶段 | `platform_intercom.c` 中的函数 / 调用行 | 封装 → SDK / 功能 |
| --- | --- | --- |
| 注册被动路由 | `demo_live_talk_task`，1233 | `demo_tirtc_set_incoming_listener` |
| HOME 准入和接管 | `live_on_conn_accepted`，361 | `demo_tirtc_adopt_incoming(LIVE, hconn)`；不调 Connect 或 WhipAccept |
| 订阅远端音频 | `live_begin_claimed_session`，745、748 | `demo_tirtc_subscribe_audio(10/14)` → `TiRtcSubscribeAudio` |
| 请求远端开麦 | 同上，769 | `demo_tirtc_send_command` → `TiRtcSendCommand` |
| 设备麦克风上行 | `live_send_uplink_20ms`，970 | `demo_tirtc_send_audio` → `TiRtcSendAudioStream` |
| 清理连接 | `live_cleanup_session`，1041 | `demo_tirtc_disconnect` |
| 归还 owner | `live_release_tirtc_session`，268 | `demo_tirtc_session_release` |

### 9.3 流和命令约定

下表的远端控制命令只对当前连接生效，且要求 `cmdw & 0x8000 == 0`；带响应位的命令只记录日志，不切换上行或挂断。

| 项目 | 当前实际行为 |
| --- | --- |
| 设备上行 | stream10，A-law / 8 kHz / 单声道，160 B / 20 ms |
| 设备下行 | 接受 stream10 或14，A-law / 8 kHz 或16 kHz；最多640 B |
| 下行订阅 | 两路都尝试；都返回错误才结束准备，提交成功不代表已收到声音 |
| `REQ_AUDIO` | 发送 `((generation & 0xffff) << 16) \| 0x1106`，payload 为单字节1 |
| 远端 `0x1106 / 0x1108` | 控制设备音频上行；无 payload 默认开，有 payload 看首字节是否非零 |
| 远端 `0x1104` | 请求结束 LIVE |
| 对端订阅 stream10 | 置 subscribed 和 remote-audio-enabled 为 true |
| 对端退订 stream10 | 清上述标志，停止上行 |
| 视频 stream11 | 订阅回调返回0仅兼容 VIEW，不发送视频帧 |

与[官方语音对讲示例](https://docs.tange.ai/products/tirtc/guides/voice-talkback.html)的客户端 stream14 约定相衔接，本工程同时保留 stream10 下行兼容。

接管时 `s_remote_audio_enabled` 默认 true，worker 以上述标志决定是否采集发送，**不把 `s_uplink_subscribed` 当作发送的硬门槛**。因此 LIVE 激活后即可尝试设备麦克风上行，不需要本地再次按键。

只移动 HOME 光标不会退出 LIVE；真正进入 AI/WX/DEV/SET 时 UI 关闭 HOME 准入，要求当前 LIVE 清理。OLED 继续使用主页，没有单独 LIVE 页面。

## 10. 四种业务的命令与音频对照

### 10.1 流格式一览

| 功能 | 建连方式 | 设备发送 | 设备接收 | 本地音频行为 |
| --- | --- | --- | --- | --- |
| AI | 主动 WHIP | stream1 / Opus / 16 kHz | stream1 / Opus / 16 kHz | 半双工，回复时停录音 |
| WX | 主动 WHIP；主叫被叫均如此 | stream0 / A-law / 8 kHz | stream0 / A-law / 8或16 kHz | 同时收发，无 AEC |
| DEV | 被叫主动 P2P、主叫接受入站 | stream10 / A-law / 8 kHz | stream10 / A-law / 8 kHz | 同时收发，无 AEC |
| LIVE | 被动接收并 adopt | stream10 / A-law / 8 kHz | stream10或14 / A-law / 8或16 kHz | 同时收发，无 AEC |

这些 stream ID 是当前业务协议约定，不是“TiRTC 所有应用必须使用”的固定编号。帧字段定义见 [TIRTCFRAMEINFO](../sdk/include/tirtc/tiRTC.h#L346)。

| 帧字段 | 本工程填写方式 |
| --- | --- |
| `stream_id` | 上表对应业务流 |
| `media` | `TIRTC_AUDIO_OPUS` 或 `TIRTC_AUDIO_ALAW`，说明 payload 的实际编码 |
| `flags` | 对应的 8 kHz 或16 kHz / 16bit / 单声道规格；不是 A-law 每个编码字节的位宽 |
| `reserved` | 清零 |
| `ts` | 单调运行时间，单位毫秒；不是右上角时钟的北京时间字符串 |
| `length` | 编码 payload 字节数，不包含帧头；Opus 长度可变 |

上行：采集 PCM → 应用编码 → 填帧头 → `demo_tirtc_send_audio()` → `TiRtcSendAudioStream()`。下行：SDK `on_audio` → runtime 路由 → 业务复制入队 → worker 解码/采样转换 → 本地播放。不能把调用 SendAudio 当作“SDK 自动录音”，也不能把收到 callback 当作“已经播放”。

### 10.2 命令字不能统一套一套位规则

| 功能 | 当前规则 |
| --- | --- |
| AI | 完整 `0x2100`；JSON `method/id/session_id` 决定业务语义 |
| WX | `0x2000` 开始、`0x2001` 挂断；接收做兼容匹配 |
| DEV | 完整 `0x2000` 确认房间、完整 `0x2001` 挂断 |
| LIVE | 低15位识别命令；`0x8000` 为该业务使用的响应标志，高16位可携带代次 |

`tiRTC.h` 还定义了 `GET_CMD`、`GET_SN`、`MAKE_CMDW`、`RESPONSE_BIT` 等通用辅助宏，但当前这些业务没有用它们构造命令。特别是通用 `RESPONSE_BIT=0x0001` 与 LIVE 自己的 `0x8000` **不是一回事**，DEV/WX 的 `0x2001` 也不能擅自解释为上一条命令的通用应答。

不要把现有探鸽业务命令“规范化”为另一套编号。新自定义业务应另行约定命令范围，并参考[官方收发命令说明](https://docs.tange.ai/products/tirtc/guides/command-messaging.html)；它不能代替当前服务的既有协议。

### 10.3 返回值也不能统一判断 `ret != 0`

| 接口类别 | 当前头文件契约 / 应用判断 |
| --- | --- |
| Init / SetOption / Start / Connect / WhipConnect | 0 为初始化、配置或提交成功；异步步骤还要等对应事件/回调 |
| SendCommand / SendAudioStream | 非负值按当前业务发送路径处理为成功，正数可表示发送字节数；不能见非零就判失败 |
| SubscribeAudio | 非负值为成功，负值为错误；不代表已经收到媒体 |
| GetSendBufferUsed | SDK 直接返回 `size_t` 字节数；无效句柄可返回0，不可单靠0证明连接健康 |
| `demo_tirtc_get_send_buffer_used` | 封装返回状态码，通过 `size_t *` 交回字节数，与 SDK 原型不同 |
| ServiceRequest | 0为成功，负数为底层错误，还可能返回 HTTP 状态或平台业务码；不能套用“只检查负数”的发送接口规则 |

## 11. 挂断、迟到回调与 SDK 软件恢复

### 11.1 普通挂断只结束会话

`demo_tirtc_disconnect()` 按 owner/generation 处理本会话：撤销 pending/expected，或在同一临界区内先成功登记 closing，再清 active；此后业务不能再取得该 active 句柄。它不关闭全局 ready。Runtime task 在 `users==0`、尚未提交断开且重试期限已到时，于 SDK 回调之外执行 `TiRtcDisconnect()`。

```mermaid
flowchart TD
    E["KEY2 / 远端结束"] --> F["Feature worker 处理信令和音频清理<br/>提交 demo_tirtc_disconnect"]
    F --> D["active 转 closing<br/>或取消本会话 pending / expected"]
    ERR["SDK on_conn_error"] --> ER["Runtime 先登记 closing、清 active<br/>再通知 Feature 并唤醒 Runtime task"]
    ER --> D
    D --> P{"有 closing 句柄、users 为0<br/>尚未提交且已到重试时间？"}
    P -->|"未满足"| W["保留回收记录<br/>等待句柄 / 使用者退出 / 重试时机"]
    W --> P
    P -->|"满足"| C["Runtime task<br/>TiRtcDisconnect"]
    C --> Z{"SDK 结果"}
    Z -->|"返回0"| CB["等 on_disconnected<br/>不再使用该句柄"]
    Z -->|"INVALID_HANDLE"| DONE["清本地回收记录"]
    Z -->|"其他错误"| RETRY["保留句柄，有限次重试 / 退避"]
    RETRY --> P
    CB --> DONE
    DONE --> FREE["最终 connect callback 等条件也排空后<br/>归还 owner，下一业务才能申请"]
```

图展示需要回收句柄的分支；只有 expected、且没有实际句柄/异步请求时，取消 expected 后即可按本地条件归还。若 connect 尚未完成，应用取消的是请求有效性，不能假装已经取消 SDK 内部 worker。

连接错误路径中，runtime 的句柄回收与 feature worker 的业务清理可分别推进，不要求先清完本地音频才断开。早到的 `on_conn_error` 也可能先交出待关闭句柄；最终 connect callback 尚未结束不必然阻止该句柄断开，但仍会阻止 owner 复用及 Stop/Uninit。

业务层等待5秒、UI 等待6秒、DEV 建连等待15秒等，都是各自的观察期限，不是 SDK 对象可以强制释放的证明。callback 长期缺失时，reservation 可能一直保留，表现为后续 BUSY。

### 11.2 需要恢复整个 SDK 时

`demo_tirtc_require_restart()` 登记恢复请求时就清 ready、记录 restart/error 并唤醒任务；它不在调用现场执行 Stop/Uninit。Runtime task 随后执行：

1. 清 ready，停止新连接准入；撤销 expected，让 active 进入 closing。
2. 处理 managed disconnect 和拒绝队列；等待连接、结果 callback、引用、入站 callback 和短准入事务排空。
3. 已经提交 Start 且未停止时，调用 `TiRtcStop()`，等待 `SYS_STOPPED`。
4. `TiRtcUninit()`，推进 generation、记录退休 owner，重新取得 identity 并启动。

drain 等待上限5秒，Stop 事件等待上限10秒；失败后保留状态，约5秒后再处理。**不是超时后强行 Uninit，更不是整机复位。** 逻辑 owner 本身不是 drain 的硬条件，避免业务等回收、回收又等业务归还的循环等待。

不能仅凭 `SYS_STOPPED` 就绕过前面的 pending/closing 检查。当前接口不提供应用可用的通用 connect-cancel/join 保证；相关限制详见[代码与流程详解](代码与流程详解.md)第25章。

例外不要漏写：Init 后选项失败、Start 立即返回错误时，当前代码直接 Uninit；此时不采用“已受理 Start 的运行期恢复”流程。

## 12. 模块七：日志、移植和链接适配

### 12.1 两套 SDK 日志

| SDK 入口 | 本地实现 | 输出特点 |
| --- | --- | --- |
| `TiRtcLogSetCallback` | `tirtc_sdk_log_callback`，runtime385 | 接收字符串和长度；不能假定字符串以 NUL 结束 |
| `tgtrp_set_log_callback` | `tirtc_transport_log_callback`，runtime417 | 接收格式字符串与 `va_list`；与高层 callback 签名不同 |
| `TiRtcLogSetLevel` | `runtime_start` | 当前构建默认15 |
| `tgtrp_set_log_level` | `runtime_start` | 当前3；`TGTRP_LOG_FLAG_STAT` 由统计日志开关控制 |
| `TiRtcGetVersion` | `runtime_start` | 启动后记录实际库报告的版本 |
| `TiRtcGetErrorStr` | runtime及部分feature的错误日志 | 只用于可读描述，不代替数值错误判断 |

`tgtrp.h` 的 `TGTRP_INTERFACE_VERSION="tagv1.5.11"` 是传输接口版本宏，不能和 TiRTC 2.3.0、应用 `APP_VERSION=01` 混为一个版本。

### 12.2 SDK 调用平台实现，是另一方向

下面这些在 [runtime 前部](../src/core/tirtc_runtime.c#L208) 实现，并由 [tirtc_app.mk](../tirtc_app.mk#L87) 接入；它们不是 `sdk/include/tirtc` 的业务 API：

| 本地适配 | 用途 / 方向 |
| --- | --- |
| `__wrap_freertos_ThreadCreateWithStackSize` | SDK 线程创建 ABI 经过本地 wrapper，再转真实平台实现 |
| `__wrap_app_log_time` | 平台时间/日志兼容入口 |
| `__wrap_printf` / `__wrap_Logf` | SDK 输出适配到本机可用日志路径 |
| `__wrap_sha256_hmac` | SDK 的 HMAC 调用接到平台 mbedTLS |
| `strdup` / `strndup` | 补齐当前运行库所需字符串分配函数 |
| `netif_list` 符号绑定 | 构建时从底包 ELF 取得地址并链接；不是业务主动调用的函数 |

公开 callback 的方向是“SDK → 应用业务通知”；上述 ABI wrapper 的方向是“SDK → 平台能力”。不要把它们全部写成应用主动调用的 TiRtc API。

移植时应配套核对头文件、静态库和底包，而非只修改 include 路径。[官方 C SDK 接入说明](https://docs.tange.ai/products/tirtc/guides/sdk-integration/c.html)也要求核对预编译包的构建契约；本项目现有 wrapper 不应原样套到不同模组/SDK版本。

## 13. 头文件里有，但当前工程没有接入的接口

### 13.1 `tiRTC.h` 尚未直接调用的13个函数

| 接口 | 未使用意味着什么 |
| --- | --- |
| `TiRtcGetBuildInfo` | 当前启动只记录版本，没有读取构建信息 JSON |
| `TiRtcConnSetUserData` / `TiRtcConnGetUserData` | 当前连接上下文由 runtime 自行管理，不绑定到这些接口 |
| `TiRtcConnSetVideoBitrateParams` | 没有视频发送码率配置 |
| `TiRtcSendMessageStream` | 应用没有直接发送流内消息；不能据此推断 SendAudio 的库内实现不用它 |
| `TiRtcSendVideoStream` | 没有视频帧上行 |
| `atomic_get_cmd_sn` | 当前业务使用各自协议的请求ID/代次，不用 SDK 命令序号生成器 |
| `TiRtcRequestKeyFrame` | 没有请求视频关键帧 |
| `TiRtcSubscribeVideo` / `TiRtcUnsubscribeVideo` | 没有主动订阅/退订远端视频；与被动视频订阅回调不同 |
| `TiRtcUnsubscribeAudio` | 业务主要通过结束连接停止整个会话，不显式发送该退订 API |
| `TiRtcLogConfig` | 没有走 SDK 文件日志配置 |
| `TiRtcWhipAccept` | 设备不是 WHIP HTTP 服务端；LIVE 的入站接管不是这个函数 |

`TiRtcSendReq`、`TiRtcSendReqWithSn`、`TiRtcSendResp` 是宏，也没有在当前业务中使用。

### 13.2 统计与底层 TGTRP

`tiRTC_stat.h` 中的三个接口全部未接入：

- `TiRtcConnGetTimeStats()`：连接层传输耗时统计。
- `TiRtcSetVideoSendMonitor()`：视频发送监控。
- `TiRtcSetConnFlag()`：连接方式选择标志；虽然放在统计头里，但不是统计查询。

`tgtrp.h` 的其他40个函数包含 init/uninit、listener、connection、channel、WHIP、媒体发送和统计，当前应用均未直接调用。底层能力由高层 SDK 管理；不要绕开 runtime 另起一套 listener/connection 生命周期。

若以后接入 `tgtrp_connection_get_time_stats()` 或 `TiRtcConnGetTimeStats()`，窗口统计是最近有效数据样本的传输耗时/RTT汇总，不应解释成“最近10次设备建连用时”。`TGTRP_LOG_FLAG_STAT` 只是日志开关，打开它也不代表应用已经调用这些查询 API。

## 14. 完整直接调用索引

此表可用于代码检索和修改影响分析。`R`＝[tirtc_runtime.c](../src/core/tirtc_runtime.c)，`A`＝[ai_chat.c](../src/features/ai_chat.c)，`W`＝[wechat_call.c](../src/features/wechat_call.c)，`D`＝[device_call.c](../src/features/device_call.c)，`L`＝[platform_intercom.c](../src/features/platform_intercom.c)。行号对应本文基线，后续改码请以函数名重新定位。

| 头文件 / API | 实际调用位置 | 所在函数或用途 |
| --- | --- | --- |
| `tiRTC.h` / `TiRtcInit` | R:2033 | `runtime_start` |
| `TiRtcUninit` | R:2098、2128、2914 | 启动前错误回收；`runtime_graceful_stop_and_uninit` |
| `TiRtcStart` | R:2123 | `runtime_start` |
| `TiRtcStop` | R:2880 | `runtime_graceful_stop_and_uninit` |
| `TiRtcSetOption` | R:1988 | `runtime_set_option`，集中承接第3章选项 |
| `TiRtcConnect` | R:2508 | `runtime_connect_submit` 的 DEVICE 分支 |
| `TiRtcWhipConnect` | R:2516 | 同一函数的 WHIP 分支 |
| `TiRtcDisconnect` | R:1082、1317 | `runtime_process_managed_disconnect` / `runtime_process_rejects` |
| `TiRtcSendCommand` | R:2576 | `demo_tirtc_send_command` |
| `TiRtcSendAudioStream` | R:2598 | `demo_tirtc_send_audio` |
| `TiRtcSubscribeAudio` | R:2614 | `demo_tirtc_subscribe_audio` |
| `TiRtcGetSendBufferUsed` | R:2635 | `demo_tirtc_get_send_buffer_used` |
| `TiRtcServiceRequest` | W:1069 | `wx_reject`；功能性直调例外 |
| `TiRtcLogSetCallback` | R:2017 | `runtime_start` |
| `TiRtcLogSetLevel` | R:2019 | `runtime_start` |
| `TiRtcGetVersion` | R:2156 | 启动完成后的库版本日志 |
| `TiRtcGetErrorStr` | R:1993、2037、2127、2894；A:1095、1271、1677；D:1601；L:427、981 | 选项、启动、停止、建连或发送错误的文字日志 |
| `tgtrp.h` / `tgtrp_set_log_callback` | R:2018 | 传输日志 callback 注册 |
| `tgtrp_set_log_level` | R:2020 | 传输日志级别和统计位 |

## 15. 从问题反查模块

| 现象 / 想改的功能 | 先看哪里 | 对应 SDK 交界点 |
| --- | --- | --- |
| 已绑定但一直无法进入 RTC ready | `runtime_start`、identity、选项、启动日志 | Init / SetOption / Start / SYS_STARTED |
| AI 有连接但没有开始对话 | `ai_send_start_session`、`ai_handle_command` | SendCommand / on_command；核对完整0x2100和JSON id |
| WX WHIP成功却没有声音 | `wx_process_connection_events`、`wx_start_audio` | 服务端0x2000、SubscribeAudio(0) |
| DEV 主叫一直等对端 | `dev_start_outgoing_call`、被叫接听路径、房间确认 | 先 expect，再 HTTP；被叫 Connect；主叫 on_conn_accepted |
| 主页 LIVE 无法接入 | UI HOME policy、`live_on_conn_accepted` | incoming 路由及 adopt，不是查设备主动 Connect |
| 收到音频但没有播放 | feature RX queue、decoder、audio_device | SDK on_audio 只交付编码数据，播放是本地职责 |
| 挂断后下一功能 BUSY | connection snapshot、pending/closing/reject、producer | Disconnect 返回与 on_disconnected 的完成边界 |
| 想获取 RTT/链路质量 | 先评估统计头API、库兼容和句柄生命周期 | 当前尚未接入，不要把日志字段当现成产品功能 |

最后记住四层完成关系：**平台业务成功 ≠ TiRTC 已连通；已连通 ≠ 协商/订阅完成；收到媒体 ≠ 已播放；界面退出 ≠ SDK 对象已释放。** 新增功能或排查异常时，先判断卡在哪一层，再定位对应调用与回调。
