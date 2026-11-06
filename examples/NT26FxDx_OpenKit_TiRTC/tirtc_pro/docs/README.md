# 文档导读

[返回项目首页](../README.md) · [应用源码入口](工程说明.md) · [构建与固件](构建与固件.md)

主文档介绍当前触屏摄像头产品，并在文末保留 Lite / Pro 共用工程的构建与依赖对照。首页按小钛参考工程组织入门路线；本文按“会用 → 看懂 → 接入 → 配置 AI 拨号 → 多人对讲”组织开发材料。

| 阅读顺序 | 文档 | 主要内容 |
| --- | --- | --- |
| ① 使用设备 | [当前设备操作](当前设备操作.md) | 准备、编译、下载、资源安装、绑定、原生 UI 配图、电话/实时查看、设置记忆、应用错误码 |
| ② 看懂工程 | [代码与流程详解](代码与流程详解.md) | 目录和任务、UI 与业务、会话状态机、音视频数据、并发安全、资源预算和全流程统计 |
| ③ 接入 TiRTC | [TiRTC 接口调用与模块流程](TiRTC接口调用与模块流程.md) | 平台与 SDK 边界、身份、连接、流号、媒体帧、能力声明、回收、SDK 错误码和移植 |
| ④ AI 发起电话 | [AI 呼叫微信与设备配置详解](AI呼叫微信与设备配置详解.md) | 云端角色/设备插件、默认视频、命令与回包、联系人匹配、兼容字段和验收 |
| ⑤ 多人对讲 | [多人对讲](多人对讲.md) | 房间业务、TiRTC 命令、G.711A 音频、PTT、资源抢占、诊断与回归 |

原“图文快速入门”内容已并入操作篇，视频位置统一在首页。操作篇保留多人对讲的入口和简要步骤，专篇用六张当前 UI 截图说明创建、加入、收听、讲话及退出，再介绍协议、声音排查及验收。首次体验无需先读全部开发文档：普通电话先手动接通，再配置 AI；多人对讲可直接按 [首次对讲](多人对讲.md#quick-start) 操作。

## 文档、代码、实测各自说明什么

- **功能和默认值**以当前源码为准；UI 的演示输入不代表真实服务器数据。
- **构建与固件**由当前工程根目录 `Makefile` 选择 `NT26FxDx_OpenKit_TiRTC`、`NT26F6D0` 和 `F6D_D`，再由项目 `config` 选择应用及构建模式。Pro 默认开启 AEC，支持 AI 边播边听和云端插话打断。固件用途、构建步骤和资源说明见 [构建说明](构建与固件.md)。
- **来源和许可**保留原始归属与授权范围，不作为当前性能承诺。
- **UI 配图**为当前源码离线渲染，原生 320×240；不是硬件采集，不证明画质、流畅度或声音效果。
- **演示录像和视频封面**来自用户提供的真实设备视频，保留横竖比例；录像没有固件版本标识，不能代替指定发布包的验收。AI 呼叫片段演示语音，当前代码默认视频策略另按配置篇验证。

## 演示素材与维护

[七段演示入口](../README.md#demo-videos) 集中放在首页，使用 GitHub 附件播放器；正文只链接到对应演示，避免复制多套素材。仓库保留原版 MP4 下载和配套封面，位于 [assets/videos](assets/videos)，来源文件名、时长、尺寸和 SHA-256 见 [manifest.json](assets/videos/manifest.json)。

| 内容 | 文档中的文件 | 时长 | 分发大小 |
| --- | --- | --- | --- |
| 设备绑定与基本信息查看 | [binding.mp4](assets/videos/binding.mp4) | 01:29 | 18.4 MiB |
| AI 对话与字幕 | [ai-talk.mp4](assets/videos/ai-talk.mp4) | 01:53 | 7.2 MiB |
| 通过 AI 呼叫微信（语音演示） | [ai-call-wechat.mp4](assets/videos/ai-call-wechat.mp4) | 00:37 | 2.9 MiB |
| 设备呼叫微信（视频） | [wechat-outgoing.mp4](assets/videos/wechat-outgoing.mp4) | 00:53 | 8.9 MiB |
| 微信呼叫设备（视频） | [wechat-incoming.mp4](assets/videos/wechat-incoming.mp4) | 00:39 | 10.1 MiB |
| 网页与微信实时查看 | [remote-view.mp4](assets/videos/remote-view.mp4) | 00:55 | 11.3 MiB |
| 多人对讲：创建、加入与房间讲话 | [group-intercom.mp4](assets/videos/group-intercom.mp4) | 01:54 | 17.2 MiB |

新增多人对讲录像来自用户提供的“多人对讲.mp4”，在 [首页对应条目](../README.md#video-group-intercom) 播放，并配合 [图文操作说明](多人对讲.md#quick-start)。录像没有固件版本标识，且含连接重试画面，不能替代指定版本的稳定性和音质验收。

七段下载视频共约 **75.8 MiB**。原有六段为 H.264 / yuv420p + 原 AAC 音轨的 720p、30 fps 分发副本，保留完整时长和画幅并前置 MP4 索引；其中最大原片约 140.9 MiB，分发副本约 10.1 MiB。新多人对讲下载文件与用户原片逐字节一致，为 540×960、30 fps、H.264 + AAC，没有转码、剪切或裁切。这些是**录像文件规格**，不是设备 MJPEG 的分辨率、帧率或码率。

受 GitHub 视频附件大小限制，设备绑定、微信呼叫设备、网页与微信实时查看及多人对讲使用额外压缩的在线播放副本，保留各自画幅、30 fps、完整时长和原 AAC 音轨。多人对讲在线播放文件约 8.3 MiB，画幅仍为 540×960；只压缩画面，AAC 数据校验与原片一致。仓库下载文件不因在线播放副本而改变，具体哈希见素材清单。

以后更新录像时：

1. 保持仓库下载链接中的稳定文件名，或同时修改首页和正文引用；重新上传 GitHub 视频附件后，同步更新首页的附件 URL。
2. 在 GitHub 页面实际点击播放器，检查能否播放、横竖方向、完整声音和首尾内容；保持原画幅比例，不硬设不匹配的宽高。不能只凭链接或播放器外观确认可播放。
3. 更新 manifest 的时长、大小和哈希；如需证明某版已验证，同时记录对应固件版本、测试步骤和结果。
4. 没有录像的功能直接链接操作说明，不设置空视频栏目。AI 呼叫现有录像继续标为语音演示；只有补充对应视频并核对内容后，才修改演示标题。

## 文档范围和核对边界

五篇主文档与本产品模块 README 共同维护；SDK 边界、来源和许可说明分别维护。公共 SDK、官方 Demo 和第三方许可证/上游通知不为文案整理而改写。上游通知的历史相对路径见 [SDK 阅读入口](../sdk/README.md) 的对应关系，旧官方教程的替代入口见 [首页利尔达教程](../README.md#lierda-guides)。

## 哪些内容以后可以共享

| 内容 | 建议 |
| --- | --- |
| TiRTC 的身份、建连、回调、媒体帧、返回值与错误码概念 | 可抽到两产品共享文档；必须注明 SDK/协议版本 |
| AI `call_contact` 角色配置与 JSON-RPC 约定 | 可共享协议讲解；各产品默认语音/视频、支持字段和接管方式分别说明 |
| 房间 assignment、connect-token、presence 与 `0x2200` 命令 | 可共享协议介绍；页面、PTT 策略、缓冲、采样率转换及错误码要按产品区分 |
| 平台发现、绑定、JWT、联系人与呼叫业务 | 同一平台协议版本可共享；鉴权、容量和超时仍要对应实现 |
| 本板操作图、按键、设置默认值、保存范围 | 本产品专属 |
| 摄像头窗口/方向、JPEG 参数、音频硬件、LCD/触摸 | 本产品专属 |
| `board_compat`、底包 ABI、资源分区、任务栈和内存预算 | 本产品专属 |
| 利尔达驱动和工具安装教程 | 可共享官方链接；编译参数和资源安装流程分别保留 |

当前文档仍分别放在各产品内，不跨产品引用业务说明。以后可单独抽取通用概念文档；具体参数、默认行为与验证记录继续按产品维护。

<a id="two-products"></a>
## 双产品构建与依赖

仓库已经按 **lite / pro** 拆分应用。lite 为 OLED 语音终端，pro 为本触屏摄像头产品；两者共享公共利尔达 SDK 源码、工具链和板级构建入口，业务代码不互相依赖。

| 项目 | lite | pro |
| --- | --- | --- |
| 应用目录 | `examples/NT26FxDx_OpenKit_TiRTC/tirtc/` | `examples/NT26FxDx_OpenKit_TiRTC/tirtc_pro/` |
| 构建入口 | 根目录 `Makefile` 与 `examples/NT26FxDx_OpenKit_TiRTC/config` | 同左，通过 `APP_VARIANT=pro` 选择 Pro |
| 构建选择 | `APP_VARIANT=lite`、`BUILD_MODE=app` | `APP_VARIANT=pro`、`BUILD_MODE=app`、`assets` 或 `full` |
| 构建命令 | 修改配置后在工程根目录运行 `make all` | 修改配置后在工程根目录运行 `make all` |
| 输出目录 | `gccout/NT26FxDx_OpenKit_TiRTC/` | `gccout/NT26FxDx_OpenKit_TiRTC/` |
| 主要烧录包 | `NT26FxDx_OpenKit_TiRTC_NT26F6D0_01.binpkg` | `NT26FxDx_OpenKit_TiRTC_NT26F6D0_pro_app.binpkg` / `NT26FxDx_OpenKit_TiRTC_NT26F6D0_pro_assets.binpkg`；`full` 另生成 `NT26FxDx_OpenKit_TiRTC_NT26F6D0_pro.binpkg` |
| 配置入口 | 项目 `config` 选择 Lite，Lite 自身构建选项位于 `tirtc/` | 项目 `config` 选择 Pro，Pro 产品选项位于 `tirtc_pro/config` |
| 底包 | 公共 `components/basePkg/F6D_D` | 同一公共 `components/basePkg/F6D_D` |
| TiRTC | lite 自己的匹配头文件和 `.a` | pro 自己的匹配头文件和 `.a` |
| 演示视频 | Lite 产品 `docs/assets/videos` | Pro 产品 `docs/assets/videos` |

Pro 使用利尔达下载工具时可按两阶段方式烧录：先将 `examples/NT26FxDx_OpenKit_TiRTC/config` 中的 `BUILD_MODE` 设为 `assets`，执行根目录 `make all`，下载输出目录 `gccout/NT26FxDx_OpenKit_TiRTC/` 中的 `_pro_assets.binpkg`；设备启动后等待 `INSTALL_OK`。再将 `BUILD_MODE` 改为 `app`，重新执行 `make all` 并下载 `_pro_app.binpkg`，正常启动后确认 `LOAD_OK`。资源数据位于 `examples/NT26FxDx_OpenKit_TiRTC/tirtc_pro/resource_store/assets14`。`BUILD_MODE=full` 另生成带外置资源镜像的 `_pro.binpkg`，仅在已确认下载工具支持本板外置 Flash 写入时使用；写入会覆盖外置文件系统。详细步骤见[当前设备操作](当前设备操作.md#build-flash)和[构建与固件](构建与固件.md)。

### 公共底包与各产品 TiRTC 库

两产品使用本工程的利尔达 SDK 和公共底包。TiRTC 库由各产品独立维护：lite 使用 `v2.5.0-87c3c290`，pro 使用 `v2.5.0-9088239c`，具体以各自来源记录和库哈希为准。不要交叉替换两产品的 TiRTC 头文件或库。

pro 的 [board_compat](../board_compat/README.md) 只保留启动清零、屏幕和链接等产品适配，不再保存私有底包、SDK 头文件或 ecapi。仍在使用的适配不能当成备份删除；精确哈希检查需在重新审计后更新，不能直接绕过。

Pro 的分区和资源布局由 `tirtc_pro/config`、板级链接配置及当前 F6D_D 底包共同决定；核对实际参数时以这些构建文件为准，不沿用旧版本的 `CODE_REGION_ADJUST` 数值。Lite 使用自己的应用及板级配置。更换底包时，应确认应用包与底包匹配并完成设备回归。两产品的 AEC 默认值由各自产品配置维护。

### 保持功能与构建互不影响

1. Lite 操作文档位于 `examples/NT26FxDx_OpenKit_TiRTC/tirtc/docs/`；Pro 操作说明见[当前设备操作](当前设备操作.md)。lite 与 pro 的 UI、按键、设置默认值和实机验收记录不同，不要混用。
2. `APP_VARIANT` 选择对应产品源码和 TiRTC 库；不要跨目录 include 或链接对方业务。当前工程的构建输出位于 `gccout/NT26FxDx_OpenKit_TiRTC/`。
3. 在项目 `config` 中同时选择 `APP_VARIANT` 和有效的 `BUILD_MODE`，然后在工程根目录执行 `make all`。当前项目配置约束支持的构建模式；Pro 使用 `app`、`assets` 或 `full`，Lite 使用普通 `app` 构建。
4. 切换产品或构建模式后，确认当前配置和产物对应；不要把其他工程生成的旧中间文件或固件包当作当前构建结果。
5. 分别编译及进行实机回归，记录固件哈希和验证范围。目录迁移与构建成功不等于重新完成全部实机验收。

## 资料入口

- [项目首页中的利尔达教程](../README.md#lierda-guides)
- [TiRTC 官方文档](https://docs.tange.ai/products/tirtc/)
- [lite 语音产品源码与操作文档](../../tirtc/docs/README.md)
- [小钛 ESP32 参考](https://github.com/tangeai/xiaotai-esp32)

硬件教程解决“板子怎样跑起来”；操作篇解决“本产品怎样用”；接入篇解决“怎样把 TiRTC 接到自己的产品”。参考工程与当前产品并不具有完全相同的 UI、媒体能力和持久化行为。
