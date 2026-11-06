# pro 的板级兼容依赖

此目录是当前 pro 应用的构建依赖，不是备份或诊断代码。
目的：两个产品共用公共 SDK，pro 只保留产品需要的启动、屏幕和链接适配。下文历史审核中的 V4 指这套 V04 触屏应用，产品目录为 `examples/NT26FxDx_OpenKit/tirtc_pro`。

## 当前基线（2026-10-09）

- 公共 SDK 使用利尔达官方 OpenKit `1.7.0`。Lite、Pro 均使用公共 `components/basePkg/F6D_D` 及公共头文件，不再使用 Pro 私有 `1.6.04_beta` 底包。
- 原 `basePkg`、`include`、`ecapi` 和 `lvgl.mk` 已退出当前构建并从发布工程移除；发布工程不需要另行准备旧 SDK。
- `tirtc_app.mk`、`runtime/runtime.mk` 保留精确哈希门禁，并在反汇编和结构布局审计后更新到本次官方 D 基线。底包 ELF SHA-256：`9553cae08079eab350557b86d5474464ffabb3086e7055739c7b57fccc930c3d`。
- Pro 使用 `CODE_REGION_ADJUST=0x4B000`：程序区 `0x2A6000..0x3BC000`（1112 KiB），内部文件系统仍为 `0x3BC000..0x3E4000`（160 KiB）。应用及底包必须成套重新构建，不能混刷 A 的旧应用镜像。
- 底包迁移后，当前 Pro 已同步 Lite 的 3A／AEC 适配，默认 `TIRTC_AUDIO_AEC_EN=y`、延迟参数 `10`。适配在 Pro 自有 `media`、AI 及音频调用处完成，官方 SDK 不改；`TIRTC_AUDIO_AEC_EN=n` 保留普通录放音路径。

| 当前保留内容 | 用途 |
| --- | --- |
| `appram.c` | 保留 ZI 清零边界修正；当前官方启动代码仍存在字节数与 32 位元素数混用问题 |
| `liot_lcdDev_ST7789.c` | 保留产品屏幕初始化和方向配置 |
| `app.ld` | 产品 RAM 段、链接布局和资源页面统计符号；分区容量来自当前 D 配置 |
| `board_compat.mk` | 使用公共官方底包，仅替换产品所需启动和屏幕目标，并保持链接顺序 |
| `licenses` | 保留历史引入依赖的许可证 |

## 本次兼容审计

| 项目 | 当前处理与依据 |
| --- | --- |
| RTOS task-create | 移除旧 `xTaskResumeAll` 补偿；新库已在创建失败和成功路径恢复调度，旧补偿会重复恢复 |
| RTOS semaphore-delete | 保留常驻对象及安全销毁逻辑；新库仍先无限等待一个 token，再删除 semaphore |
| 摄像头 DMA fence | 保留；CSPI/Camera 反汇编与旧版相同，回调表仍为 20 字节，frame-end 与 DMA-end 仍共用通知，必须另等 DMA 完成 |
| JPEG/video | 保留；两库反汇编及 JPEG 头文件未发生相关契约变化，解码仍写入调用者提供的行缓冲区 |
| Preferences | 保留 `LFS_stat` 原错误码路径；新底包仍经串行任务返回 LittleFS 结果，stat 字段偏移未变；NVM 内部对象尺寸由新库自行管理 |
| 普通 HTTP | 保留已有失败 OPEN、队列重置与 CLOSE 后释放处理；`libliot_https.a` 反汇编与旧版相同，下层 HTTP 库仅日志常量变化 |
| 音频 | 默认 3A 流式录放音，保留 Pro 的 0 档静音和增益曲线；采用 Lite 的先静音并等待约 200 ms 排空、再停录音和播放、重建采集及音量复位纠正。关闭 AEC 时保留原 Pause/Resume/Stop 及对齐处理 |
| TiRTC runtime | 保留 SHA256 枚举、日志、线程等适配；已核对的 RTOS/lwIP/mbedTLS 类型布局与旧版一致；`netif_list` 由当前 ELF 提取，不能沿用旧地址 |

构建参数和固件用途见 [构建说明](../docs/构建与固件.md)。

## 公共基准（2026-09-23 历史记录）

- 本地：`F:/Lierda_Dev/org_0915/CAT1.bis_OpenCPU-main`，SDK 1.6.0。
- 小钛：[xiaotai-lierda](https://github.com/tangeai/xiaotai-lierda/tree/f159bfd3e82cc514dfe6b5b2269b2c96fa8e8f93)，固定提交 `f159bfd3e82cc514dfe6b5b2269b2c96fa8e8f93`。
- 校验范围：`components/`、`config/`、`rules/`、根 `Makefile`、`build.bat`、`examples/Makefile`。
- 上述范围共 4,541 个文件，逐文件内容与本地 `_0915` 一致，无内容修改、缺失或额外文件；残留空目录也已清理。
- 小钛仓库另外提供的 mbedTLS 许可证保存在本目录 `licenses`；其 `APP_PACKAGE_MEMMAP` 打包参数本工程不需要，公共打包 Makefile 已恢复本地原版。
- 全目录复核后，其他官方 Demo 的差异已恢复、缺失文件已补齐。没有引入利尔达后续 SDK 更新。

本节的本机路径和文件数是 2026-09-23 同步审核记录，不是读者需要准备的目录。固定参考提交用于追溯，当前构建以上面的 2026-10-09 基线为准。合并后的公共打包 Makefile 曾保留 lite 所需的 `APP_PACKAGE_MEMMAP` 参数；上面的“已恢复本地原版”仅描述迁移前 pro 独立工程。后续变更不包含在此历史统计中。

## 历史私有 SDK 整理（2026-09-23，已被当前基线替代）

当时 V4 已验证版本使用的底包是 1.6.04_beta，和公共 1.6.0 并不是同一套二进制。
音频配置、MQTT 结构、摄像头 DMA、RTOS 兼容和网络符号地址都依赖确切底包。
直接用旧 `.a` 覆盖，会让接口与实际运行代码不匹配；因此把这套依赖移入 V4，原样保留。
`basePkg/F6D_A` 共 53 个文件约 65.26 MiB，完整保存配套底包，避免新旧库混用。

| 内容 | 用途 |
| --- | --- |
| `basePkg/F6D_A` | 整理前已验证的库、AP/CP/bootloader、内存布局和打包信息，字节未改 |
| `include`、`ecapi/PLAT` | 与底包匹配的音频、MQTT、JPEG 和芯片接口，内容未改 |
| `appram.c` | 保留启动清零修正，避免把字节数当作 32 位元素数造成越界 |
| `liot_lcdDev_ST7789.c` | 保留当前屏幕初始化和方向配置 |
| `app.ld` | 保留内存布局、RAM 代码段和资源页面统计符号 |
| `lvgl.mk` | 用公共 LVGL 源码编译，保留有效源文件列表和 Windows 归档路径处理 |
| `board_compat.mk` | 仅在 V4 构建中选择上述依赖，保留原链接顺序 |

当时公共 SDK 的源文件保持原状；V4 的 Makefile 为启动和 ST7789 两个目标文件选择本目录实现。
当时匹配的私有头文件优先加入 V4 包含路径，底包目录由 V4 单独指定。当前构建已移除此私有 SDK 选择；TiRTC 仍为 `sdk` 下的 2.5.0。

## 验证（2026-09-23）

- V4 正常应用全量 ARM 编译、链接、打包通过。
- 外置资源安装程序编译、链接、打包通过。
- 315 个编译目标文件与整理前逐字节一致。
- 最终 `.elf` 与 `.bin` 均逐字节一致；打包文件仅偏移 12、13 的两个包头字节不同，应用和底包有效载荷相同。
- APP：1,045,776 字节；静态 RAM 范围：488,244 字节；分区不变。
- APP SHA-256：`3cb724c701d31f6e3c288136c1fe0219fceb9ff23ae9fb4a5442eb51f2ba2d5f`。
- ELF SHA-256：`3a58aafa46d0781a95429024a657b6a4df254f530dff615bdb42958676fa234e`。
- 本次未烧录设备，也未重新进行实机通话测试。

## 当前 lite / pro 构建

对外使用根目录 `examples\NT26FxDx_OpenKit\tirtc_pro\build.bat`，生成含外置资源的出厂初始化包 `firmware/pro/YYYY_MM_DD_HH_MM_SS_pro.binpkg`。其应用构建仍由 `examples\NT26FxDx_OpenKit\tirtc_pro\build_app.bat` / 板级 Makefile 选择 `APP_VARIANT=pro`，中间产物在 `gccout/tirtc_pro`。pro 的配置是 `tirtc_pro/config` 文件，业务、`board_compat`、`sdk` 和 `runtime` 均保留在自己的目录。

两产品共用当前官方 SDK、F6D_D 底包与工具链，不互相引用业务，不复用编译对象。后续升级仍应重新核查接口兼容及实机行为；不要通过删除哈希检查或盲目替换哈希来强行通过编译。

上面“验证（2026-09-23）”是合并前的记录；当前构建方式见 [构建与固件](../docs/构建与固件.md)，软件检查见 [测试说明](../tests/README.md)。完整说明见 [双产品构建与依赖](../docs/README.md#two-products)。
