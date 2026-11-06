# 许可与第三方声明

本目录位于利尔达 OpenCPU SDK 工程内，用于构建 NT26FxDx OpenKit V04 的 TiRTC Pro 应用。应用主体迁自探鸽 [xiaotai-lierda 仓库](https://github.com/tangeai/xiaotai-lierda/tree/f159bfd3e82cc514dfe6b5b2269b2c96fa8e8f93/examples/NT26FxDx_OpenKit/tirtc_pro)；公共 SDK 和部分板级适配来自利尔达，其他依赖来自各自上游。此文件按集成关系列出许可查阅入口，代码归属仍以每个文件的版权、来源和许可声明为准。

本目录 [LICENSE](LICENSE) 是探鸽应用源码所采用的 MIT 许可文本，其中列明探鸽的版权主体。它不代表利尔达对探鸽应用代码主张版权，也不覆盖利尔达原厂代码、第三方组件或供应 SDK。迁入本 SDK 工程不会改变任何文件原有的版权和许可。

## 许可范围

| 工程内容 | 许可和来源说明 |
| --- | --- |
| 利尔达公共 OpenCPU SDK、原厂示例与工具 | 以仓库根目录 [LICENSE](../../../LICENSE)、[工程说明](../../../README.md)、[中文说明](../../../README_ZH.md) 及各文件通知为准。供应二进制、工具和第三方组件可能有独立条款。 |
| 探鸽 TiRTC Pro 应用源码与文档 | 以文件中的版权、来源和 SPDX 标记为准；适用本目录 [MIT 许可](LICENSE) 的文件按该许可使用。迁移和集成不改变探鸽的版权归属。派生文件、独立组件及下列依赖按各自条款处理。 |
| 板级及显示适配 | `board_compat/` 中的文件按文件头及 [板级依赖说明](board_compat/README.md) 标注的来源和许可使用。`port/tirtc_port.c` 源自利尔达 Demo，适用 Apache-2.0；其他板级适配文件不因目录位置而自动采用同一许可。 |
| TiRTC SDK 头文件和预编译库 | `sdk/include/tirtc/` 与 `sdk/lib/libTiRTC.a` 按 TiRTC SDK 自身授权使用，不属于应用 MIT 许可范围。随工程提供这些构建依赖不改变其授权，详见 [TiRTC SDK 分发说明](sdk/README.md)。 |
| 平台和 runtime 代码 | 该部分主体源自探鸽工程；利尔达工程集成适配及各文件改动按版权头和来源记录区分。固定参考版本、改动范围与历史许可见 [平台来源记录](platform/SOURCE_PROVENANCE.md) 与 [runtime 来源记录](runtime/SOURCE_PROVENANCE.md)。 |
| Opus 1.4 | AI 音频路径使用仓库公共 Opus 源码，遵循其 BSD 风格许可及专利授权声明，详见 [COPYING](../../../components/thirdparty/opus/opus-1.4/COPYING)。应用侧 `media/opus_codec.*` 适配代码的许可以文件头为准。 |
| littlefs | `storage/littlefs/` 中的源码遵循随附的 [BSD-3-Clause 许可](storage/littlefs/LICENSE.md)，来源和本地改动见同目录 `PROVENANCE.json` 与补丁文件。 |
| UI、LVGL、二维码及其他公共组件 | `ui/` 中带有独立许可的文件按其许可使用，例如 [UI 许可](ui/LICENSE)。公共组件以对应组件目录、文件头和上游许可文件为准。 |
| 字体及生成字库 | 字体许可不由应用 MIT 许可替代。字体来源和生成信息见 [字体来源说明](ui/tools/font-sources/SOURCE.md) 及清单。 |
| 历史上游许可副本 | `platform/LICENSES/`、`runtime/LICENSES/`、`ai/UPSTREAM_LICENSE` 和 `sdk/UPSTREAM_THIRD_PARTY_NOTICES.md` 用于保留对应来源的许可或原始通知。旧路径和版本说明按其原工程语境理解；组件当前适用范围以源码标记和本表对应说明为准。 |

## SDK 与双产品边界

当前 Lite 与 Pro 示例共用根目录的利尔达公共 SDK、`components/basePkg/F6D_D` 和工具链，但应用源码、TiRTC 预编译库及产品依赖分别管理。Lite 的许可范围见 [Lite 第三方声明](../tirtc/THIRD_PARTY_NOTICES.md)。

本工程随 Pro 示例提供构建所需的 TiRTC SDK 文件。该分发安排不把 TiRTC SDK 转为开源软件，也不扩大供应方许可允许的使用或再分发范围。底包、驱动库、预编译组件和工具的许可需分别依据其随附声明及供应条款确认。

## 来源和通知维护

2026-09-28，按探鸽项目维护方要求，将探鸽应用中原标记为 Apache-2.0 或 `MIT AND Apache-2.0` 的探鸽部分统一为 MIT。此处理仅针对相应探鸽应用源码及其许可标记，不改变利尔达原厂代码、TiRTC SDK、Opus、littlefs、字体或其他第三方材料的版权和许可。原始版权通知、固定来源记录和历史许可副本仍保留在对应目录。

分发源码或二进制时，应一并保留适用的许可证文本、版权声明、来源通知和第三方声明。不得仅依据本文件推定某个文件属于应用 MIT 范围；如文件头、专用许可或供应协议另有说明，应按该文件或组件的具体条款处理。
