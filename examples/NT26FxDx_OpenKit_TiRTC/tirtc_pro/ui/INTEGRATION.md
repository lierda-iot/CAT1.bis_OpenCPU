# 当前触屏 UI 接口约定

面向替换页面或增加业务的开发者。产品操作与原生配图见 [操作篇](../docs/当前设备操作.md)，任务和业务流程见 [代码篇](../docs/代码与流程详解.md)。本文件仅定义 UI 与后台的接合规则。

## 1. 显示与任务边界

- 逻辑画面 **320×240 横屏**；RGB565，`LV_COLOR_DEPTH=16`、`LV_COLOR_16_SWAP=0`。
- 面板原生尺寸 240×320，由 `port` 和板级 ST7789 配置完成横屏方向；不通过拉伸 UI 改变宽高比。
- `port` 的 GUI 任务是唯一 LVGL 所有者，调用 `tirtc_ui_init/process`、LVGL 定时处理和同步 LCD flush。
- 后台使用 `tirtc_ui_publish_*()` 复制数据；不能从 SDK 回调、网络、文件或录音任务调用 LVGL。
- 正常固件定义 `TIRTC_EXTERNAL_UI_ASSETS`，资源工作任务读取并验证字体/背景，再供 UI 使用。GUI 不执行 SPI 文件 I/O。

## 2. 动作从 UI 到业务

`user_main.c` 注册 `app_ui_action()`。动作包含 `type/page/index/value/text/extra`；后端返回 0 仅表示接受请求，最终状态异步发布。

| 动作 | 实际后端 |
| --- | --- |
| AI_START / AI_STOP | `ai`；开始前检查 calls/LIVE 占用 |
| REFRESH_CONTACTS | `contacts` 请求刷新 |
| CALL_AUDIO / CALL_VIDEO / ACCEPT / REJECT / HANGUP | `calls`，复用既有通话状态机 |
| SET_SPEAKER_VOLUME / SET_MIC_GAIN / 两个音频开关 | `preferences` 更新 RAM，再发布给 UI 与媒体业务 |
| NETWORK_REFRESH / NETWORK_RECONNECT | `network`，同时通知平台 |
| BIND | `platform` 绑定重试 |
| REMOTE_END 等 | `remote`，使用当前会话代次 |
| SET_IDLE_EXPRESSION / SET_SLEEP_MINUTES / SET_ACK_VOICE | 本地运行态；当前不持久化，回应声尚无提示音服务 |
| ROOM 系列 | `group`，后台执行关系同步、创建/加入/退出、连接和按住讲话 |

动作回调不等待 HTTP、NVM、录音或连接退出。页面导航的目标放在 `action.value`；仅定义枚举并不证明该业务已实现。

## 3. 状态从后台到 UI

| 接口 | 所有者与内容 |
| --- | --- |
| `publish_network` | 4G 注册、移动数据、RSRP/RSRQ/SNR/RSSI 与信号格 |
| `publish_platform` | 绑定、API/MQTT/RTC 状态；不含凭据 |
| `publish_contacts` | 完整联系人缓存、加载和过期标记 |
| `publish_call` | 会话 ID、呼入/呼出、类型、对端、通话状态 |
| `publish_remote` | LIVE 代次、订阅与有效开关、远程状态 |
| `publish_room` | 房间关系、连接、成员与开麦状态；不携带密码或连接凭据 |
| `publish_ai` | AI 状态、字幕、段落、音频诊断 |
| `publish_audio_settings` | 已接受的音量/增益/开关，防止旧媒体快照回滚设置 |
| `publish_resources` / `publish_system` | 独立资源、时间、模组与事件快照 |
| `publish_state` | 聚合初始状态；业务更新优先使用各自专用邮箱 |

字符串必须终止，UTF-8 按完整码点裁剪。生产者不得把 token、密钥或整个平台响应放进 UI。不要以一份过期的完整状态覆盖其他服务所有的数据。

初始化顺序：先恢复 preferences，再启动 UI 和后台。通用 `ui_state_defaults()` 不是产品最终配置：实际默认音量为 8/10、麦克风增益 8/10，两开关开启；保存值优先。

## 4. 页面与会话

- HOME/CLOCK/MENU/DIAGNOSTICS 可保持 AI；进入其他页面会请求停止 AI。
- 首页表情点击启动 AI；再次点击不是结束开关。
- 通讯录从服务器取得最多 12 个联系人，点击时保存稳定 ID 与类型，不把可变列表索引作为最终拨号依据。
- 普通 AI 呼叫默认视频；首页绿色电话快捷操作仍是第一位微信联系人的语音呼叫。
- 通话进入 CALL 时记录此前页面：若为 ROOM 或 ROOM_FORM，通话结束、忙或失败后返回 ROOM 并重新同步房间；其他来源进入 CALL_RESULT。媒体仍在释放时可回到 HOME 等待；后台完成后不重新切换用户页面。该规则包含房间页收到来电，房间页本身没有主动拨号入口。视频通话显示远端画面，不显示本机预览。
- 正常 LIVE 接入保持 HOME/CLOCK，显示底部提示和红色结束按钮。`TIRTC_PAGE_REMOTE` 枚举仍存在，但正常产品流程不跳入此页面。
- ROOM/ROOM_FORM 进入多人对讲；表单返回 ROOM，ROOM 返回 MENU。离开房间页面范围先停麦、暂停媒体，服务端房间关系保留；点击“退出房间”并确认后才提交解除关系。PTT/退出动作携带房间号及会话代次，旧页面动作不能影响新连接。Room 的 HTTP、TiRTC 命令和音频队列见 [多人对讲](../docs/多人对讲.md)。
- 未绑定时由平台状态决定绑定向导；普通断网不应假装解绑。
- `tirtc_ui_get_rendered_page()` 只在页面完成成功 LCD 刷新后报告可用页面，供远程接入门禁检查。请求跳页成功不等于画面已经刷出。

### 多人对讲页面约定

- 菜单入口为“更多功能 → 多人对讲”。未分配房间时显示“创建房间”“加入房间”；表单仅接受数字，加入需六位房间号，密码为空或四位数字，使用键盘“确认”提交。
- 请求受理只置提交状态，不直接改写房间号、分配关系或成员。受理后立即清空密码输入；失败重试须重新填写。离开 ROOM_FORM 会同时清空房间号和密码。
- PTT 按钮可用时显示“按下讲话”，按住时为“松开结束讲话”；关麦显示“麦克风已关闭”，开关开启但增益为 L0 时显示“麦克风静音（L0）”，尚不可讲话时显示“等待房间连接”。仅当前已知、已分配、已连接、非忙碌会话且麦克风开启、增益非零时可按住发送。
- 按住时约每 200 ms 续期；松手、移出按钮、页面删除、会话变化、关麦、增益变为 L0 或续期中断会停止当前 PTT。触摸未松开时恢复增益也不能恢复旧手势，必须松手后重新按下；切回页面或重连同样不会恢复之前的按住状态。
- 成员每页三人，最多显示前 12 位；人数超过上限时明确标为“前 12 位成员”。成员使用稳定标识，展示名称、本机标记及“正在讲话”状态；页面数量不是独立测量的总在线人数。
- 房间业务使用音频，不显示视频。[多人对讲实机录像](../README.md#video-group-intercom) 集中在首页；设备操作和人工验收步骤见 [操作篇](../docs/当前设备操作.md#group)，当前检查结果见 [验证范围](../docs/多人对讲.md#validation)。

## 5. 视频帧契约

`publish_frame(pixels,width,height)` 接收小端紧密排列 RGB565，最大 **208×144**；当前视频模块实际发布 **192×144**。320×240 是屏幕大小，不是该接口允许的输入缓冲大小。

- 返回前已复制数据，生产者可以复用自己的缓冲。
- `NULL,0,0` 清帧；非法组合/超限返回 -2。
- 只保留最新待处理帧，允许替换旧帧，不能保证每次发布都显示。
- 页面切换和会话变化会清理原帧；触摸按住时，视频帧仍可继续更新，页面重建则按输入状态安全处理。
- 接口不带 session ID；视频生产者必须检查 owner/generation，停止旧会话后不再发布旧帧。
- 每次 flush 在 `lv_disp_flush_ready()` 前调用 `tirtc_ui_video_flush_done(success,last)`，保持统计与实际 LCD 传输结果一致。

`published` 是发布次数，`consumed` 是 GUI 接收次数，`lcd_frames` 是成功刷新中包含的不同视频帧；flush 区域数不是视频帧率。完整统计见 [性能说明](../docs/代码与流程详解.md#performance)。

## 6. 设置与校验

音频四字段走内部 NVM 双槽保存；表情、息屏时间、回应声 UI 选择仅本次运行有效。设置保存不是 UI 自己写 Flash，见 [preferences](../preferences/README.md)。

设置页“麦克风音量”下拉选项发送 `SET_MIC_GAIN`，可选择 L0（静音）～L10。仅请求被接受后更新显示，后台用 `publish_audio_settings` 同步实际值；下拉列表展开时不以后台刷新覆盖正在选择的选项。默认麦克风档位为 8，已有保存值优先。

修改 UI 后至少检查：文字完整、4:3 布局、触摸对应、设置不被旧快照回滚、按住空白处视频继续、主被叫方向正确、远程结束后恢复待机，以及 Room 表单校验、密码清理、PTT 松手/失焦停止、成员翻页和来电结束返回。离线渲染只能检查布局，不能替代 LCD/触摸/音视频实测。
