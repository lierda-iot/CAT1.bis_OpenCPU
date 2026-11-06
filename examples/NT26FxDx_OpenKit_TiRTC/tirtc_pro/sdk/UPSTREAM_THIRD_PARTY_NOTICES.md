<!-- SPDX-FileCopyrightText: 2026 Shenzhen Tange Intelligent Technology Co., Ltd. <https://tange.ai> -->
<!-- SPDX-License-Identifier: Apache-2.0 -->

# 第三方组件与发布边界

本文件说明构建 `tirtc` 时会直接或间接使用的第三方组件。各组件仍受自己的
许可证和授权约束；本项目的 SPDX 声明不会覆盖或重许可它们。

| 组件 | 本工程位置/用途 | 许可证或授权状态 | 发布要求 |
| --- | --- | --- | --- |
| Lierda OpenCPU SDK | `tirtc` 的 RTOS、网络、HTTP、MQTT、音频、OLED API 和底包 | 仓根声明 Apache-2.0；供应二进制保留各自条款 | 保留[根许可证](../../../LICENSE)、[应用许可证副本](LICENSES/Apache-2.0.txt)及文件通知；本次随包包含 F6D_A 底包 |
| Tange minimal-system examples | TiRTC 生命周期、绑定和会话流程参考 | MIT，Copyright (c) 2026 探鸽智能 | 保留 [MIT 版权和许可全文](LICENSES/MIT.txt) |
| TiRTC SDK | `sdk/include/tirtc/*`、`sdk/lib/libTiRTC.a` | proprietary / 单独 SDK 协议 | 本次随包提供编译必需的 SDK；不得标成 Apache/MIT，后续使用和分发仍遵循 SDK 授权 |
| cJSON | JSON 解析和生成，当前 F6D_A 链接预编译 `libcjson.a`，应用增加解析前深度保护 | MIT，Copyright (c) 2009-2017 Dave Gamble and cJSON contributors | 保留 [cJSON.h 文件头中的 MIT 通知](../../../components/thirdparty/CJSON/cJSON.h) |
| Opus 1.4 | AI 上下行语音编解码 | BSD 3-Clause 风格许可证，版权主体见上游 `COPYING` | 保留完整 [COPYING](../../../components/thirdparty/opus/opus-1.4/COPYING) |
| Mbed TLS 2.28.10 | HMAC、SHA-256、Base64 | Apache-2.0 | 保留 [LICENSE](../../../components/thirdparty/mbedtls/LICENSE) 和各文件版权通知 |

## TiRTC SDK 特别说明

本次发布已确认 TiRTC SDK 和 F6D_A 底包允许随工程公开分发，以保留可直接编译
和烧录的完整依赖。TiRTC SDK 文件按供应形态原样保留，SDK 头文件和静态库不受
应用仓的 MIT/Apache-2.0 许可证覆盖；SDK 和底包的其他使用、修改及再分发条件
仍以各自授权为准。本说明不扩大这些组件的许可范围。

当前文件身份（2026-09-14 核对；TiRTC `v2.5.0-87c3c290` 配套交付，替换此前旧版摘要）：

| 文件 | 大小/摘要 |
| --- | --- |
| `sdk/lib/libTiRTC.a` | SHA-256 `D0807049076CA7D718DBE691F5FE8893B00132EB91F70B6C2FB738A54AAE99E3` |
| `sdk/include/tirtc/tiRTC.h` | SHA-256 `B1315121ABF843C43C669F754382CAB302C3B0B35BDFE41F4C4756EF95636293` |

摘要只能证明当前字节身份，不能证明 SDK 版本或授权。后续更换 SDK 时，应重新
核对交付版本、目标 ABI 和分发条件，并更新上述摘要。

## 本次发布布局

应用源码、许可通知和编译所需 SDK 一并保留：

```text
tirtc/
├─ src/ include/ config/ board/ docs/       # 按文件 SPDX 开源
├─ LICENSES/                                # Apache-2.0 和 MIT 全文
├─ THIRD_PARTY_NOTICES.md
├─ SOURCE_PROVENANCE.md
└─ sdk/
   ├─ include/tirtc/                        # 随包的匹配头文件
   └─ lib/libTiRTC.a                        # 编译所需静态库
```

当前构建仅链接 `sdk/lib/libTiRTC.a`。两项应用异常防护没有
改写 TiRTC、cJSON 或 F6D_A 供应静态库；平台任务创建适配的版本门禁见
[代码与流程详解第 4.2 节](docs/代码与流程详解.md#42-tirtc_appmk)。

仓库根目录的 `LICENSE` 不替代 SDK、底包及第三方组件自身的许可证或授权。
