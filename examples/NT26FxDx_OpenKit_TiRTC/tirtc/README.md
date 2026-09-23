<!-- SPDX-FileCopyrightText: 2026 Shenzhen Tange Intelligent Technology Co., Ltd. <https://tange.ai> -->
<!-- SPDX-License-Identifier: Apache-2.0 -->

# NT26FxDx_OpenKit_TiRTC TiRTC application

This directory contains the product application for the Lierda NT26F6D0
OpenCPU board.  It provides the three-key OLED UI, LTE/network time, device
binding, permanent MQTT, AI chat, WeChat voice calls via the “探鸽小钛” mini program,
device-to-device voice calls, platform LIVE intercom and shared ES8311 audio.

The product code is intentionally separate from Lierda's demonstration tree:

- `src/app`: task composition only;
- `src/core`: one process-wide TiRTC lifecycle and connection owner;
- `src/services`: LTE, binding and MQTT transports;
- `src/features`: independent AI/WX/DEV/LIVE state machines;
- `src/media`: shared audio and codecs;
- `src/ui`: key input, OLED primitives and menu policy;
- `include`: public interfaces between those layers;
- `sdk`: the unmodified, version-locked TiRTC header/archive;
- `board` and `config`: target IO and build-time product policy.

## Documentation

- [当前设备操作与 UI 状态说明](docs/当前设备操作.md)
- [代码与流程详解](docs/代码与流程详解.md)
- [TiRTC 接口调用与模块流程](docs/TiRTC接口调用与模块流程.md)
- [源码来源记录](SOURCE_PROVENANCE.md)
- [第三方组件与发布边界](THIRD_PARTY_NOTICES.md)

Application-source licensing is declared per file. The supplied TiRTC headers
and binary SDK are separate proprietary materials and are not relicensed by
this repository. This release includes the matching SDK required to build the
application. Component licences and redistribution boundaries are documented in
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

The historical migration notes and the old 20-second record/playback test are
not part of this product tree.  Hardware demonstrations remain available in
the upstream Lierda examples.

## 默认构建

在 SDK 根目录执行：

```powershell
make all PROJECT=NT26FxDx_OpenKit_TiRTC MODEM=NT26F6D0
```

Build-time log policy is kept in the project `config`. The common modem rules
automatically map `MODEM=NT26F6D0` to `MODEMPKG=F6D_A`. Restricted-network mode
is intentionally fixed in the runtime adapter for this cellular target; see
[代码与流程详解](docs/代码与流程详解.md).
