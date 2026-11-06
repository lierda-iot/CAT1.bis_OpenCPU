<!-- SPDX-FileCopyrightText: 2026 Shenzhen Tange Intelligent Technology Co., Ltd. <https://tange.ai> -->
<!-- SPDX-License-Identifier: MIT -->

# TiRTC SDK placement

The headers and prebuilt archive below are proprietary TiRTC SDK materials.
They are not covered by the MIT or Apache-2.0 licences applied to the product
application source:

- `include/tirtc/*`
- `lib/libTiRTC.a`

They may be used or redistributed only under the applicable TiRTC SDK agreement
or separate written authorization from the TiRTC owner. This release includes
the matching headers and archive required to build the application; public
distribution of this supplied SDK has been confirmed for this release. This
does not relicense the SDK or expand the rights granted by its own agreement.

The upstream release's notices are retained verbatim in
[UPSTREAM_THIRD_PARTY_NOTICES.md](UPSTREAM_THIRD_PARTY_NOTICES.md).
Its component paths describe the upstream tree, not this reduced integration.
File hashes and local adaptation provenance are recorded in
[SOURCE_PROVENANCE.md](../runtime/SOURCE_PROVENANCE.md).
A hash identifies bytes; it is not a grant of rights or proof of version
provenance. Integration details are in [runtime/README.md](../runtime/README.md).

## 阅读上游原始通知

`UPSTREAM_THIRD_PARTY_NOTICES.md` 按上游原文保留，其中相对链接按原工程目录解释，不能从本文件所在目录直接解析；其中的 OLED 等产品说明按原工程理解。当前 Pro AI 由应用侧 Opus 适配器调用公共 Opus 1.4 源码，使用 16 kHz 单声道；微信、设备电话、LIVE 和 Room 保持 G.711 A-law。TiRTC SDK 负责媒体传输，不代替这些应用侧编解码。

当前可用的许可入口：

- [当前应用 MIT 许可](../LICENSE) / [应用许可范围](../THIRD_PARTY_NOTICES.md)
- [上游 MIT 副本](../runtime/LICENSES/MIT.txt)
- [历史上游 Apache-2.0 副本](../runtime/LICENSES/Apache-2.0.txt) / [利尔达原厂许可](../../../../LICENSE)
- [cJSON 许可头](../../../../components/thirdparty/CJSON/cJSON.h)
- [Opus 1.4 上游许可（当前 AI 编解码使用）](../../../../components/thirdparty/opus/opus-1.4/COPYING)
- [保留的 mbedTLS 许可](../board_compat/licenses/mbedtls-LICENSE)

以上提供当前应用与历史上游许可的阅读路径；原始供应 SDK、历史通知和上游许可副本均保持原文。
