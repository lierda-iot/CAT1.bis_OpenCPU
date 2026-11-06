# 外置 Flash 的 UI 资源

## 默认用法：先安装资源，再烧正式程序

使用原版利尔达工具时，按 [Pro 固件与两次烧录说明](../../../../firmware/pro/README.md) 下载配套的 `_pro_assets.binpkg` 和 `_pro_app.binpkg`：

1. 先全量下载资源安装包，让设备正常启动，等串口输出 `[assets14] INSTALL_OK files=7 ...`。
2. 再全量下载正式 Pro 程序包，重启后确认 `[assets14] LOAD_OK ...` 和正常界面。

**顺序固定，不能在第一次安装完成前刷第二包。** 两包都由工具写入内部 Flash；第一包启动后通过设备 SPI 驱动安装外置资源，第二包替换临时安装器并读取这些资源。安装器没有正常业务界面，也不会自动切换到正式应用。

本方案不需要修改原厂烧录工具。全量下载可能清除内部绑定和设置，请先保存需要保留的信息；外置资源安装范围见下文。原来的 `build.bat` 仍可生成固件与外置文件系统合并包，但包内包含 `EFID01` 不等于所有工具都会写入它；只有已验证支持本板外置初始化与写入的工具，才采用合并包一次下载。合并包写入外置镜像时会覆盖外置 Flash 文件。

## 资源内容

- `/ui14-font.bin`：631,201 字节，原始 4 bpp 字形。
- `/ui13-bg.bin`：153,600 字节，原始 RGB565 背景。
- `/ui14-meta.bin`：70,108 字节，全部 7,010 字符的索引和字形尺寸数据，使用明确的小端格式。
- `/ui48-digits.bin`：4,495 字节，时钟和绑定码的原始数字字形。
- `/ui-phone.bin`、`/ui-wechat.bin`：原始电话图标与微信二维码图片。
- `/ui-face-v1.bin`：A/B 两套表情姿态和动画时序；文件不保存运行时指针。
- `asset_catalog.py` 固定完整资源清单，`asset_manifest.h` 保存各文件长度和 CRC32，加载时全部校验。
- 当前应用按字形缓存字体，背景常驻；没有把完整字库复制到 RAM。
  缓存大小以 `font_cache.h` 中 `TIRTC_FONT_CACHE_BYTES` 为准。
- 新迁移的索引、数字字形、图标和动画参数在启动时加载到固定 BSS；不新增动态堆分配，也不在界面绘制过程中读取这些文件。只有全部加载、校验通过才进入正常 UI。内置基础英文字体继续用于启动和资源故障提示。

**从旧版升级必须同时更新配套资源。** 原先只有字形和背景两个文件的设备，不能只刷新版正式程序包；使用匹配的安装器补齐资源后再烧正式程序。缺失、长度错误或 CRC 不匹配会保留资源错误提示，不使用不完整数据绘制界面。

## 开发维护：重新提取资源与独立安装器

在工程根目录执行：

```powershell
python examples/NT26FxDx_OpenKit/tirtc_pro/resource_store/prepare_external_assets.py
.\examples\NT26FxDx_OpenKit\tirtc_pro\build_assets.bat
```

生成工具读取当前字库和背景源码，更新 `asset_manifest.h`、`installer_blob.c`
以及 `examples/NT26FxDx_OpenKit/tirtc_pro/resource_store/assets14`。字库原件和生成工具仍在 `ui/tools`；图标和表情原始数据保留在非外置构建分支。提取工具不会压缩字形、改变像素或修改表情参数。

脚本对当前字库的长度和 SHA-256 有固定检查，以上命令用于重建**这一套已确定的资源**，不是任意换字体后都能直接执行的通用打包器。更换字库或背景时，应同步审核源数据、脚本的长度/哈希条件、manifest 与安装程序，并重新生成和验证应用/资源包；不要只去掉断言继续发布。

独立安装程序校验已有文件，仅写入缺失或不匹配的上述资源；同步、关闭并读回校验七项资源，完成后打印
`[assets14] INSTALL_OK files=7 updated=N skipped=M`。`updated=0 skipped=7` 也表示成功。出现 `INSTALL_FAILED` 时停止，并保留错误信息；下载器的“下载成功”不能代替安装成功日志。

存储驱动会初始化经两遍全片检查确认为空白的外置 Flash；已有有效文件系统按原有保护规则使用，未知或损坏数据不会自动清空。安装器没有自动重试写入、删除其他文件腾空间的行为。

安装完成必须重新烧录正常应用包。安装器构建产物位于 `gccout/tirtc_pro_assets/TiRTC_pro_Assets_NT26F6D0_pro_release.binpkg`，正式程序位于 `gccout/tirtc_pro/TiRTC_pro_NT26F6D0_pro_release.binpkg`；当前配套发布副本分别使用 `_pro_assets.binpkg`、`_pro_app.binpkg` 后缀，放在根目录 `firmware/pro/`，具体文件见该目录 README。正式程序包不包含外置镜像；工具额外擦除选项和内部绑定数据仍需单独确认。

正常应用不包含资源安装入口和完整资源初始化大数组；它从外置文件区读取，
保留文件系统保护、CRC 校验、失败重试和字体缓存逻辑。

两步安装和支持外置写入的合并包流程，最终都让正式应用从外置 Flash 读取同一套资源，不会把完整字库常驻应用 RAM。

### 出厂打包的检查

`build_factory_image.py` 使用仓库自带的 `lfsutil` 生成 4 MiB、4 KiB 块的 LittleFS 2.0 镜像，检查几何信息并逐文件提取、比对资源。`package_factory.py` 使用自带 `fcelf` 合包，检查五项载荷的地址、容量、SHA-256，以及前四项与正常应用包一致后，才更新根目录 `firmware/pro/YYYY_MM_DD_HH_MM_SS_pro.binpkg` 和校验清单。文件名取对应应用 `.bin` 的编译完成时间（UTC+8），不改动内部应用版本或载荷。

当前打包使用公共 `components/basePkg/F6D_D` 的 bootloader、AP 和 CP，应用区为 `0xAA6000` / 1112 KiB。应用扩容后，内部文件系统仍从物理地址 `0x3BC000` 开始、容量 160 KiB。出厂脚本会拒绝旧 A 底包或旧布局的正常固件包；迁移后必须先完整重建 Pro，再运行出厂打包。外置资源仍是原来的 4 MiB 文件系统，出厂初始化时覆盖外置文件的行为保持不变。

中间文件保存在 `gccout/tirtc_pro_factory`。两个工具只在电脑上生成文件，不访问设备；包内容检查不代替实机外置 Flash 烧录验证。

## 与文档配图的关系

正常固件使用外置资源；文档离线渲染使用生成这些资源的同一份字体和背景源码，因此画面布局与素材一致。渲染图不验证 SPI 挂载、CRC 或上电安装；实机仍需确认资源安装成功。

返回 [操作篇资源安装](../docs/当前设备操作.md#assets)。
