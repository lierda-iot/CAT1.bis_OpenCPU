# 当前 SDK 与应用边界

本文从利尔达 SDK 工程维护角度说明公共 SDK、F6D_D 底包与 TiRTC Pro 应用的集成边界。当前 `tirtc_pro` 应用主体迁自探鸽 [xiaotai-lierda 仓库的固定版本](https://github.com/tangeai/xiaotai-lierda/tree/f159bfd3e82cc514dfe6b5b2269b2c96fa8e8f93/examples/NT26FxDx_OpenKit/tirtc_pro)。源码中的版权、许可和来源声明决定各文件的归属，不因迁入利尔达 SDK 目录而改变。逐项改动见 [运行时来源记录](runtime/SOURCE_PROVENANCE.md)、[平台来源记录](platform/SOURCE_PROVENANCE.md) 和 [第三方声明](THIRD_PARTY_NOTICES.md)。

## 1. 公共 SDK 基线

当前工程以仓库根目录的公共 OpenCPU SDK 为基线：

| 层次 | 当前路径 | 作用 |
| --- | --- | --- |
| 公共组件 | `components/` | 利尔达驱动、系统库、第三方组件和通用外设实现 |
| F6D_D 底包 | `components/basePkg/F6D_D/` | NT26F6D0 的底包、分区和配套链接输入 |
| 通用构建规则 | `rules/` | 工具链、编译、链接、镜像和 binpkg 打包规则 |
| 工程入口 | `examples/NT26FxDx_OpenKit_TiRTC/Makefile`、`config` | 选择 Lite/Pro、构建模式和当前模组配置 |

公共层提供利尔达 SDK 的接口和目录约定。Pro 目录承载迁入的探鸽 TiRTC 应用主体、配套库与资源，以及为当前板卡集成所需的适配；该目录不是公共 SDK 的替代版本。

当前 Pro 通过 `MODEMPKG=F6D_D` 使用公共底包。不同版本的底包、头文件和预编译库不能混用；升级公共 SDK 后，应重新检查 [板级兼容说明](board_compat/README.md) 和 [运行时来源记录](runtime/SOURCE_PROVENANCE.md)。

## 2. 当前工程接入

根工程通过 `APP_VARIANT` 选择应用目录：

| 配置 | 应用目录 | 用途 |
| --- | --- | --- |
| `APP_VARIANT=lite` | `examples/NT26FxDx_OpenKit_TiRTC/tirtc/` | Lite TiRTC 示例 |
| `APP_VARIANT=pro` | `examples/NT26FxDx_OpenKit_TiRTC/tirtc_pro/` | 本 Pro 触屏音视频示例 |

Pro 目录由 `examples/NT26FxDx_OpenKit_TiRTC/Makefile` 接入。该 Makefile 负责：

1. 包含根工程配置和 Pro 配置；
2. 将 Pro 的源码、`board_compat`、`runtime`、TiRTC SDK 和资源目录加入构建；
3. 根据 `BUILD_MODE` 生成应用包、资源安装器包或包含外置资源文件系统的完整包；
4. 将最终产物统一写入 `gccout/NT26FxDx_OpenKit_TiRTC/`。

不选择 `APP_VARIANT=pro` 时，不应通过修改公共 SDK 文件来启用 Pro；应从工程入口传入配置，例如：

```sh
make all APP_VARIANT=pro BUILD_MODE=app MODEM=NT26F6D0 MODEMPKG=F6D_D
```

`BUILD_MODE=app` 生成不含外置资源文件系统的正式应用包；`BUILD_MODE=assets` 生成资源安装器；`BUILD_MODE=full` 在应用包基础上生成当前板级完整包。产物用途和烧录顺序见 [构建与固件](docs/构建与固件.md)。

## 3. Pro 目录中的适配内容

以下目录说明当前构建的职责，不代表目录内所有文件由同一方创作。应用主体来自探鸽 TiRTC Pro 工程；利尔达来源的板级文件、第三方组件及供应库按各自通知和许可处理，逐项来源见 [许可和第三方声明](THIRD_PARTY_NOTICES.md)：

| 目录 | 当前职责 |
| --- | --- |
| `board_compat/` | NT26F6D0/V04 板级链接布局、启动修正和 ST7789 屏幕适配 |
| `runtime/` | TiRTC 连接生命周期、线程、错误回调和底包兼容封装 |
| `platform/` | 设备绑定、MQTT、网络状态和本板级平台接口 |
| `media/` | ES8311 音频、AEC/ANS、G.711 A-law 和 AI 使用的 Opus 路径 |
| `resource_store/` | 外置 Flash 资源清单、资源安装器和完整包生成脚本 |
| `sdk/` | 与本应用匹配的 TiRTC 头文件和预编译库；许可边界见该目录说明 |
| 其他业务目录 | UI、AI、电话、实时查看、多人对讲、存储和设置功能 |

这些文件通过当前 Pro 的 `tirtc_app.mk` 和 `runtime/runtime.mk` 参与链接。它们不是备份文件，删除或替换时必须同步检查链接脚本、内存布局、底包 ABI 和资源包格式。

## 4. 与外部来源的关系

部分 TiRTC 接口封装和业务协议参考了外部 TiRTC 资料或历史实现。当前目录中的来源、许可和实际改动分别记录在：

- [运行时来源记录](runtime/SOURCE_PROVENANCE.md)
- [平台来源记录](platform/SOURCE_PROVENANCE.md)
- [TiRTC SDK 分发说明](sdk/README.md)
- [应用许可范围](THIRD_PARTY_NOTICES.md)

外部平台、服务端协议和 TiRTC 文档只用于接口或业务联调参考。当前工程的编译路径、板级参数、资源布局和固件打包方式以本仓库的利尔达 SDK 工程为准，不依赖外部业务仓库目录。

## 5. SDK 升级检查

升级公共 SDK、F6D_D 底包或 TiRTC 预编译库时，至少重新检查：

1. `board_compat` 的链接地址、RAM/Flash 分区和启动清零边界；
2. RTOS、lwIP、mbedTLS、音频和摄像头接口的结构体布局及回调约定；
3. `runtime` 中的线程、错误回调、断连和资源回收顺序；
4. `media` 中录放音停止、AEC/ANS、缓冲区所有权及编解码参数；
5. `resource_store` 的外置 Flash 地址、资源数量、安装器日志和完整包格式；
6. 各精确哈希门禁及对应的反汇编或实机验证记录。

哈希检查用于阻止未经审核的二进制组合，不能通过删除检查或直接改写哈希值来绕过。本文只记录工程边界；SDK 升级是否可用，仍需完成当前工具链构建及目标板验证。

