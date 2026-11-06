# 小钛体验平台适配

入口为 `tirtc_platform.h`。启动一个优先级 8、16 KiB 栈的后台任务；主任务每 5 秒调用 `tirtc_platform_publish()`，只复制缓存，不访问模组或服务器。UI 操作仅合并重试请求，不执行网络 I/O。

## 实际流程

1. 等待网络快照的当前 SIM、CID1、IP 与有效 UTC 时间。使用网络 `generation` 识别承载变化；旧代次响应不再作为成功处理。
2. GET `http://ep-open.tangeopen.com/services`，读取并校验服务地址。
3. 无本地身份：POST `/v1/device/report`，其中 `mac` 是 IMEI 派生标识。显示返回的六位码，按上游协议使用本地 190 秒窗口。
4. 临时 MQTT 用 `temp_client_id` / `temp_token`，订阅 `device/{temp_client_id}/cmd`，等待 `auth_grant`。
5. 校验云设备 ID、设备密钥，写入 `tirtc_device_auth.nvm`，读回并检查结构、CRC 和内容，然后发布授权 ACK。写入或验证失败时不确认授权。
6. 用 UTC 秒数、随机 nonce 和设备密钥签名 POST `/v1/device/token`。签名使用真实 HMAC-SHA256；收到有效 `mqtt_token` 才认定平台 API 已鉴权。
7. 正式 MQTT 使用 `sn_{device_id}`，订阅命令和通知两个主题。连接与两项订阅完成后才报告平台在线。
8. 开机已有身份时直接重新鉴权。410 / 6006 或远程解绑进入有签名的重新绑定流程，保留持久化身份；临时掉线不会当成解绑。

当前媒体运行时由 `runtime` 独立启动，平台提供已绑定身份、业务请求与消息分发；AI、calls、LIVE、Room 各自处理自己的会话。IMEI 不作为云端下发的设备 ID。身份读取仅限后台生命周期所有者，可能查询 IMEI/ICCID，不能从 UI 调用或打印凭据。

Room 使用正式 CALL 服务上下文访问 `/v1/call/group/device/*`。正式命令主题中的 `type=room_assignment_changed` 将顶层版本/过期字段交给 Room，后者只触发关系重查，不直接开麦。鉴权、入房及媒体租约见 [多人对讲](../docs/多人对讲.md#connection-flow)。

## 设备信息与媒体能力

[device_metadata.h](device_metadata.h) 集中维护设备信息，由现有 `/v1/device/profile` 请求上报：

| 平台字段 | Pro 上报值 |
|---|---|
| 芯片型号 `hardware.chip_model` | `NT26F6D0`（模组型号） |
| 开发板型号 `hardware.board_model` | `L-CT4IT00-YP00W-01-V04` |
| 固件版本 `firmware_version` | `F6D_D_pro_0.1.0` |

业务版本当前为 `0.1.0`；构建包名继续使用 `APP_VERSION=pro_release`，设备状态页读取的 SDK 模组版本也保持独立。烧录包含本次修改的固件后，已绑定设备在开机联网鉴权成功、媒体运行时就绪后沿原流程更新 profile，无需解绑重绑；升级前的下载包不会自动获得这些字段。

设备互呼、实时查看和微信的现有媒体声明保持 G.711 A-law 与 MJPEG，不声明 H.264。实时查看 `profiles.stream` 显式上报上行音频 `up_audio_streamid=10`、上行视频 `up_video_streamid=11`、下行音频 `down_audio_streamid=14`；不支持实时查看视频下行，因此不填下行视频 ID。设备互呼和微信的 Stream ID 沿用各自协议，不能添加只属于实时查看的字段。接口字段见[官方设备 profile 契约](https://github.com/tangeai/tirtc-server-example/blob/main/thing-connect/api-reference.md#post-v1deviceprofile)。

## 本地 SDK 约束

- 当前随包 F6D_D HTTP 入口的 SIM 选项未实现，因此明确只支持活动 SIM0。SIM1 返回 -131，不悄悄换用另一张卡。
- 当前体验服务发现返回 HTTP / MQTT；本模块只接受这两种已实现方案，不把 HTTPS 降级或忽略证书验证。这些链路没有 TLS 的传输保密性，不是生产私有部署的安全配置。
- 不移植不同 SDK 版本的 MQTT 接收缓冲区私有结构补丁。MQTT 通知与业务 HTTP 响应采用各自容量限制，不通过 MQTT 搬运音视频；业务请求容量见后文。
- MQTT 的 deinit 返回 0 或 -2 都不能单独证明任务退出。`tirtc_mqtt_compat.c` 针对固定本地库核查终态，并要求当前任务优先级低于 SDK TX 的 22；构建前必须通过该库 SHA256 门禁。请烧录配套完整 binpkg。
- 所有 MQTT 初始化只能由同一后台所有者串行执行。临时客户端未清理完成会阻止正式初始化，正式客户端未清理完成也会阻止临时初始化。每次最多等待 5 秒，超时保留句柄、参数、凭据及同步对象，后续只继续检查，不重复提交已接受的 deinit。
- 临时回调使用代次标记、入口开关和在途计数；关闭后排空再删除信号量。SDK 异常回调没有不可变代次参数，因此临时异常回调不改变当前连接状态，所有者依靠 SDK 状态和有界超时检测故障。
- HTTP 的异步 CLOSE 未完成时保留上下文，由清理任务或后续请求继续回收。不能把 `stop` 返回当作可释放内存的证明。
- 只对已无回调/等待者的信号量补一个 token 后删除，规避本地 SDK 删除包装内部永久等待空信号量的问题。

## 状态与数据

`api_ready` 表示签名身份验证通过；`mqtt_connected` 表示正式连接及必需订阅通过。服务发现成功、4G 有 IP、临时 MQTT 在线均不代替上述状态。绑定码在本地到期或网络代次变化后被隐藏。日志不输出密钥、token、授权 JSON、HTTP 正文或原始 IMEI。

UI 使用独立平台邮箱，不覆盖系统、资源、网络或业务邮箱。`binding_required` 只在没有持久化身份或确认解绑时使绑定向导重新出现；临时掉线不会强制跳页。

## 文档与验证入口

产品上手见 [设备操作](../docs/当前设备操作.md)，平台/SDK 边界和时序见 [接入篇](../docs/TiRTC接口调用与模块流程.md)。固件用途与构建方式见 [构建与固件](../docs/构建与固件.md)，实机结果须按对应固件单独验收，不把早期主机测试结果写成当前全部功能的结论。

来源和许可证见 [SOURCE_PROVENANCE.md](SOURCE_PROVENANCE.md) 及 `LICENSES/`。

## 受保护的业务 HTTP 请求

`demo_binding_service_request_guarded()` 供独立后台联系人/呼叫/Room 任务使用，按服务枚举选发现端点，正式设备 JWT 不暴露给 UI。请求必须带 `demo_binding_service_context_t`，在锁等待、遗留 HTTP 清理、实际发送前及响应后复核身份/凭据/路由代次。旧响应清零，不能变为新账号的操作。POST 请求体上限 2047 字节，足够本板完整设备媒体 profile；UI 不调用此接口。


业务 HTTP 响应上限 8192 字节，按调用者容量在同一异步上下文分配块尾部申请，AI 原 4096 字节调用不扩大。超限返回错误并清空响应，绝不把截断 JSON 当成功。释放仍等待 SDK 清理完成，同时清零整个上下文和尾部响应。
