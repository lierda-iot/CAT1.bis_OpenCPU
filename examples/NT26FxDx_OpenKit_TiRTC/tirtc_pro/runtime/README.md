# TiRTC 运行时与单路会话管理

本目录由固定上游 managed runtime 适配而来，来源/许可见 [SOURCE_PROVENANCE.md](SOURCE_PROVENANCE.md)。当前加载应用 `sdk` 目录的 TiRTC 2.5.0，使用本产品 `board_compat` 的配套底包；音视频能力由上层 AI/calls/LIVE/Room 复用，不是早期只启动 SDK 的占位模块。

完整接入流程见 [TiRTC 接口篇](../docs/TiRTC接口调用与模块流程.md)，下面只保留生命周期与内存约束。

## 调用顺序

1. `tirtc_runtime_start_service()` 启动唯一管理任务（64 KiB、优先级 10）。等待网络/时间与正式绑定身份。
2. 设置 SDK 日志和 `MAX_SEND_BUFFER=128 KiB`，后者必须早于 Init；Init 后设置单连接等选项，Start 并等待 ready。
3. 业务长期注册 listener；所有会话操作携带 owner/generation，使用 claim/connect/whip/expect/adopt/send/subscribe/disconnect/release 包装接口。
4. 回调数据只在回调期间有效，业务做有界复制，后续解码/播放/HTTP 不在 SDK 回调执行。
5. 断开先撤销句柄可用性，等待在途引用，收到断开并清理后归还会话；未完成不能被下一会话强行复用。
6. 网络或凭据变更先拒绝旧会话新操作，按 Stop → SYS_STOPPED → Uninit → 新身份重启。SDK 重启不等于板子复位。

| owner | 业务 |
| --- | --- |
| 0 | NONE |
| 1 | AI |
| 2 | 微信 |
| 3 | 设备通话 |
| 4 | LIVE 实时查看 |
| 5 | GROUP_ROOM 多人对讲（启用 `HWDEMO_GROUP_ROOM_EN` 时） |

Room 的 WHIP、订阅流 1、入房确认和电话抢占交接见 [多人对讲流程](../docs/多人对讲.md#connection-flow)。加入业务房间不等于已经获取本层媒体使用权。

当前最大媒体连接数为 1。相同数值句柄被后来连接复用，也必须通过 generation 区分。不要绕过本层从 UI 或业务直接调用全局 Stop/Uninit。

## 与底包绑定的兼容层

`runtime.mk` 配套静态库、头文件、链接 wrapper 和哈希门禁。任何库/底包升级都要重新核对 ABI；不要删除哈希检查只求链接成功。

当前 `rtc_thread` 请求栈从库内的 8192 提升到 16384；该库转换为 FreeRTOS 深度后在此底包实际分配 32 KiB。业务管理任务的 64 KiB、这部分线程栈、128 KiB 发送上限不是 SDK 的总 RAM，另有连接、TLS、收发队列和解析对象。

运行时性能必须结合剩余堆、最大连续块、帧阶段计数和发送积压判断；有空闲堆不代表不存在 CPU/调度/驱动等待瓶颈。

## 返回值和验证

完整接口见 [tirtc_runtime.h](tirtc_runtime.h)。不同 SDK API 的返回语义不同，例如命令发送与媒体发送不能统一用 `ret==0`。见 [返回值说明](../docs/TiRTC接口调用与模块流程.md#api-reference)。

默认关闭周期调试，保留必要错误。配套固件用途与构建方式见 [构建与固件](../docs/构建与固件.md)；文档不能替代重复建连、取消、主被叫和异常断网的实机回归。
