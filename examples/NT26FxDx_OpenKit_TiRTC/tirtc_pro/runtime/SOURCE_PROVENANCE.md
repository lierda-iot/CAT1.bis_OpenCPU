# 运行时来源与适配证据

- 上游：https://github.com/tangeai/xiaotai-lierda
- commit：`f159bfd3e82cc514dfe6b5b2269b2c96fa8e8f93`（2026-09-15）。历史源路径为 `examples/L_CT4IT00_YP00W_01_V04/tirtc/src/tirtc_runtime.c` 及 `include/tirtc_runtime.h`，不是当前产品目录。
- 原版权和来源通知保留。上述历史版本使用 `MIT AND Apache-2.0`；2026-09-28 按维护者要求，探鸽自有修改与已有 MIT 参考代码统一为 MIT，见 [当前应用许可](../LICENSE) 和 [应用许可范围](../THIRD_PARTY_NOTICES.md)。本目录 `LICENSES/` 保留历史许可全文，不改变供应 SDK 的授权。
- 2026-09-23 按用户提供的 `tirtc__eigencomm-ec71x__gcc-arm-none-eabi-10-2020-q4-major__v2.5.0__mini` 原样更新库和配套头文件，SHA-256：`31c72553a082104b28eb8a2baa3e837b87afd25a54c2330eadc640dd0f287a5d`。内部版本为 `v2.5.0-9088239c` / `f72f5d3c`；此前工程实际为 `v2.5.0-87c3c290`，并非 2.3。5 个公共头文件字节相同。

## 当前底包基线（2026-10-09）

公共 SDK 更新为用户下载的官方 `org_1009` 版本（构建版本 `1.7.0`），Lite/Pro 统一选择 `components/basePkg/F6D_D`。Pro 不再覆盖公共 SDK 头文件或底包目录。当前底包 ELF SHA-256 为 `9553cae08079eab350557b86d5474464ffabb3086e7055739c7b57fccc930c3d`；TiRTC `libTiRTC.a` 及其公共接口未升级。

本次实际比较旧 Pro 与新 D 的反汇编和 ELF DWARF，核对的 23 类 RTOS/lwIP/mbedTLS 类型及枚举布局一致，包括 TCB（200 字节）、Queue（80 字节）、netif（200 字节）、addrinfo（32 字节）、SSL context（200 字节）及 SSL config（164 字节）。构建继续从当前 ELF 提取 `netif_list`，本版本地址为 `0x0806f514`，该数值只作为审计记录，不写死在源码中。

新 D 的 `mbedtls_md_info_from_type` 仍只接受 `3..8`，SHA256 枚举为 `6`，因此原生 TiRTC 的 `sha256_hmac` 包装仍需要保留。线程、日志和生命周期包装也继续保留。新 Liot task-create 在失败时已经恢复调度，旧的额外 `xTaskResumeAll` 包装已移除；这与 TiRTC 的线程栈大小包装是不同问题。

这些结论仅适用于上述确切版本。已更新现有精确哈希门禁，没有删除或绕过检查。静态审计不代表已通过实板通话、视频或网络恢复测试。

## 本地适配

1. 单任务启动入口，保留成功创建的常驻 semaphore / queue，失败允许重试；新 D 的 semaphore-delete 反汇编仍与旧 A 相同，删除空 semaphore 会先永久等待，因此保留原有处理。
2. 复用现有绑定身份和网络快照，以 SIM、route generation、binding generation 及独立的非零 credential_epoch 防止旧连接/回调跨身份继续可用；同身份同路由的重新绑定也不能沿用旧媒体会话。
3. 保留上游句柄引用、延迟断连、错误回调和待完成连接上下文管理，不在 SDK 回调里销毁 SDK。
4. 原生库与当前工具链链接选用 `-fno-lto`；精确哈希变化时 Makefile 停止编译，要求重新审查。
5. 保留 `Logf`、`printf`、`app_log_time` 安全日志包装；凭据类原生日志被过滤，受控日志不打印 token、签名或完整身份。
6. 保留 `tgtrp_connection_set_on_error` ABI 适配以及 `freertos_ThreadCreateWithStackSize` 栈包装。历史上独立存在的 Liot task-create 失败恢复 wrapper 已在本次迁移中移除，因为新 D 已修复相应缺陷。
7. `sha256_hmac` 用当前 mbedTLS 头文件语义选择 SHA256。固定 SDK 的原生调用传枚举 9，而本地底包 `mbedtls_md_info_from_type` 仅接受 3..8；本地 SHA256 是 6，直接调用会返回空。
8. `strdup/strndup` 使用兼容本项目 RTOS 堆的分配路径，不让 native SDK 字符串复制落入未配置的 newlib sbrk 堆。

2026-09-23 更新 TiRTC 时，新库的 native 平台、加密、日志和 RTC 线程对象的可加载节与之前相同。
`TiRtcConnect`、`_tirtc_create_conn`、`_on_error`、`TiRtcDisconnect`、
`TiRtcWhipConnect/Accept`、`TiRtcSendAudio/VideoStream` 的反汇编及调用目标相同，
因此当时保留既有适配，仅将设备呼叫失败清理的版本门禁扩展到上述 TiRTC 构建；本次底包迁移没有更新该 TiRTC 库。
历史二进制对照产生了原件、哈希清单与反汇编证据；这些诊断产物不作为当前开源应用的构建依赖随包提供。保留的上游原文中的旧哈希不代表当前库，当前库身份以上述记录和构建门禁为准。

升级 SDK 时应针对固定版本的实际库重新生成反汇编与测量记录，不能沿用其他底包或 SDK 版本的结论。
