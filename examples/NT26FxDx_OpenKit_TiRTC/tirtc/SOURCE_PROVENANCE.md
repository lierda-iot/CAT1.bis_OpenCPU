<!-- SPDX-FileCopyrightText: 2026 Shenzhen Tange Intelligent Technology Co., Ltd. <https://tange.ai> -->
<!-- SPDX-License-Identifier: Apache-2.0 -->

# 源码来源记录

本文记录 `tirtc` 产品目录的来源边界，供公开发布、代码审查和后续升级使用。
它是工程溯源记录，不替代权利人的授权文件或法律意见。

## 1. 本地产品代码

本目录由探鸽智能基于利尔达 NT26F6D0 OpenCPU SDK 完成板级适配，并接入探鸽
TiRTC、ThingConnect、AI、微信通话、设备通话及平台 LIVE 对讲能力。

文件实际许可证以文件头的 `SPDX-License-Identifier` 为准：

- 独立新增的板级适配、媒体、网络、UI、构建和文档代码使用 `Apache-2.0`；
- 包含探鸽公开参考实现表达及本项目 Apache 修改的文件使用
  `MIT AND Apache-2.0`；
- 利尔达派生文件继续使用其上游 `Apache-2.0`，保留利尔达归属并标记探鸽修改；
- `sdk/` 下的 TiRTC 头文件和静态库不属于上述开源授权。

## 2. 利尔达派生文件

以下文件基于本 OpenCPU SDK 中的板级示例修改，保留原始利尔达归属：

| 本地文件 | 上游文件 | 本地修改范围 |
| --- | --- | --- |
| `src/app/app_entry.c` | `demo/src/0demo_main.c` | 从硬件 demo 入口改为产品任务编排和依赖门控 |
| `src/ui/key_input.c` | `demo/src/demo_key.c` | 增加防抖、按键语义和 ISR 到 UI 队列 |
| `include/key_input.h` | `demo/inc/demo_key.h` | 改为产品模块声明 |
| `src/ui/ssd1306_display.c` | `demo/src/demo_lcd_ssd1306.c` | 改为单一显示 owner 和 framebuffer primitive |
| `include/ssd1306_display.h` | `demo/inc/demo_lcd_ssd1306.h` | 改为显示服务 API |
| `board/iodriver.ini` | 产品变体根目录 `iodriver.ini` | SPI0 换脚，避开 I2C1 OLED 引脚 |

上游仓根 `LICENSE` 为 Apache License 2.0。发布时不得删除这些文件中已有的
利尔达版权、作者或来源说明。

## 3. 探鸽公开参考实现

实现流程参考：

- 仓库：`https://github.com/tangeai/tirtc-device-example`
- 审计时参考提交：`cb183be67f10b4cad7895a5be7a6799e10cc4281`
- 目录：`minimal-system-examples/esp32-s3`、`minimal-system-examples/esp32-p4`
- 上游应用许可证：MIT，`Copyright (c) 2026 探鸽智能`

明确存在代码或接口表达重合的核心文件：

- `src/core/tirtc_runtime.c`
- `include/tirtc_runtime.h`
- `src/features/device_call.c`

按相同公开协议流程移植并保守保留 MIT 义务的文件：

- `src/features/ai_chat.c`、`include/ai_chat.h`
- `src/features/wechat_call.c`、`include/wechat_call.h`
- `src/features/device_call.c`、`include/device_call.h`
- `src/features/platform_intercom.c`、`include/platform_intercom.h`
- `src/services/device_binding.c`、`include/device_binding.h`
- `src/services/formal_mqtt.c`、`include/formal_mqtt.h`
- `src/core/tirtc_runtime.c`、`include/tirtc_runtime.h`

这些文件当前标为 `MIT AND Apache-2.0`：MIT 约束覆盖保留的上游表达，
Apache-2.0 覆盖本产品新增修改。若权利负责人决定统一重许可，必须把书面授权、
对应源版本和变更提交一并更新到本文件，不能只机械改 SPDX 行。

## 4. 独立适配文件

本次比对未发现与公开参考仓有显著逐行重合、按当前工程策略使用
`Apache-2.0` 的文件包括：

- `src/media/*`、对应媒体头文件；
- `src/services/network_manager.c`、`include/network_manager.h`；
- `src/ui/ui_controller.c`、`include/ui_controller.h`；
- `config`、`tirtc_app.mk`；
- `README.md` 和 `docs/` 下的本项目文档。

“未发现逐行重合”不等于专利、商标或协议不受约束。后续若从其他来源复制实现，
应在合入时更新本清单。

## 5. 供应 SDK

以下文件由 TiRTC SDK 单独提供，保持字节不变，不添加 Apache/MIT 文件头：

- `sdk/include/tirtc/basedef.h`
- `sdk/include/tirtc/tgtrp.h`
- `sdk/include/tirtc/tiRTC_stat.h`
- `sdk/include/tirtc/tiRTC.h`
- `sdk/lib/libTiRTC.a`

探鸽公开参考仓的 SDK NOTICE 明确说明：TiRTC SDK 头文件和预编译静态库是
proprietary materials，不受仓库 MIT 许可证覆盖；进一步分发、修改和生产使用
受 SDK 协议或单独书面授权约束。本次发布已确认 TiRTC SDK 和 F6D_A 底包可以
随工程公开分发，因此保留编译必需的 SDK 头文件、静态库及底包。随包提供不改变
各组件的许可归属，也不扩大其使用和再分发授权；见
[第三方组件与发布边界](THIRD_PARTY_NOTICES.md)。

## 6. 发布前溯源检查

1. 将参考仓的浮动 `main` 换成确认过的 release tag/commit。
2. 由权利负责人确认每个 `MIT AND Apache-2.0` 文件的最终许可策略。
3. 确认当前 `libTiRTC.a` 的来源、版本、目标 ABI 和公开再分发权。
4. 重新计算 SDK、ELF、BIN、BINPKG 的 SHA-256，并更新对应清单。
5. 保留 `LICENSES/`、`THIRD_PARTY_NOTICES.md` 及所有第三方原始通知。
6. 运行 SPDX/REUSE 扫描；供应 SDK 在外部 SBOM 中标为 `NOASSERTION` 或其真实许可，
   不得伪标 Apache/MIT。
