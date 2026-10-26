# Flash 工具使用指南

## 概述

`download.py` 是用于 Lierda OpenCPU 设备的统一烧录工具，支持完整固件烧录和仅应用程序烧录两种模式。当前仅支持 Windows 下烧录。

## 环境要求

- Python 3.6+
- Python 依赖包：`pyserial`（必需），`py7zr`（可选，用于 .7z 解压）
  - 安装命令：`pip install pyserial` 或 `pip install pyserial py7zr`
- FlashToolCLI 工具（支持 .zip 或 .7z 格式，自动解压）
  - .7z 格式优先使用项目自带的 `tools/7z/7z.exe` 工具
  - 如果未找到 7z.exe，则尝试使用 py7zr 模块
- 通过 USB 或 UART 连接目标设备

## 命令行用法

### 基本语法

```bash
python download.py <pkg_path> --type <full|app> --mode <usb|uart> [--port <PORT>] [--erase]
```

### 必需参数

- `pkg_path`: `.binpkg` 文件路径（必需）
- `--type`: 烧录类型，必须为 `full` 或 `app`（必需）
- `--mode`: 下载模式，`usb` 或 `uart`（必需）

### 可选参数

- `--port`: 指定 COM 端口（例如 COM7）。USB 模式下未指定时自动检测，若模块固件异常导致无法枚举 USB AT 口，则需要手动进入 boot 模式并指定烧录端口（boot 引脚拉高后复位模组即可进入）。
- `--erase`: 烧录前执行完整分区擦除（仅全量烧录模式，会擦除射频校准区域外的其他分区，包括文件系统，系统配置区域）。

### 示例

**完整烧录 - USB 模式自动检测端口：**

```bash
python download.py firmware.binpkg --type full --mode usb
```

**完整烧录 - USB 模式指定端口：**

```bash
python download.py firmware.binpkg --type full --mode usb --port COM7
```

**完整烧录 - UART 模式（必须指定端口）：**

```bash
python download.py firmware.binpkg --type full --mode uart --port COM7
```

**仅应用程序烧录：**

```bash
python download.py firmware.binpkg --type app --mode usb --port COM7
```

**完整烧录并擦除分区：**

```bash
python download.py firmware.binpkg --type full --mode usb --port COM7 --erase
```

## 使用 Makefile

### 必需参数

`pkg` 和 `type` 参数都是**必需的**：

```bash
make burn pkg=<file.binpkg> type=<full|app> mode=<usb|uart> [port=<PORT>] [erase=1]
```

### Makefile 示例

**完整烧录 - USB 模式：**

```bash
make burn pkg=firmware.binpkg type=full mode=usb
make burn pkg=firmware.binpkg type=full mode=usb port=COM7
```

**仅应用程序烧录：**

```bash
make burn pkg=firmware.binpkg type=app mode=usb port=COM7
```

**完整烧录 - UART 模式：**

```bash
make burn pkg=firmware.binpkg type=full mode=uart port=COM7
```

**带芯片擦除：**

```bash
make burn pkg=firmware.binpkg type=full mode=usb port=COM7 erase=1
```

### 使用 build.bat

```bash
.\build.bat burn pkg=gccout\app\app_NT26F6D0_01.binpkg type=full mode=usb
.\build.bat burn pkg=gccout\app\app_NT26F6D0_01.binpkg type=app mode=usb port=COM7
```

## 烧录类型详解

### 完整烧录 (`--type full`)

烧录包含所有分区的完整固件：

1. 使用 `pkg2img` 解压包
2. 检测并探测设备
3. 配置产品设置
4. 烧录所有镜像（若配置擦除参数，则会执行擦除操作）
5. 系统复位

### 应用程序烧录 (`--type app`)

仅烧录应用程序二进制文件：

1. 使用 `pkg2img` 解压包
2. 读取 `imagedata.json` 获取 PKGFLXAPP 信息
3. 探测设备
4. 擦除应用程序 flash 区域
5. 烧录应用程序二进制文件
6. 系统复位

**注意：** 应用程序烧录需要 `pkg2img` 命令生成的 `imagedata.json` 文件。

## 下载模式

### USB 模式 (`--mode usb`)

- 若设备固件异常导致无法枚举 USB AT 口，则需要手动操作使设备进入下载模式
- 未指定端口时可自动检测
- 比 UART 模式更快

### UART 模式 (`--mode uart`)

- 必须明确指定端口
- 使用串行通信
- 比 USB 模式慢

## 端口自动检测（仅 USB 模式）

当 USB 模式下未指定 `--port` 时：

1. 首先搜索 "Lierda At Port"
2. 如果未找到，提示用户进入下载模式
3. 等待 "Lierda QDLoader Port"（10 秒超时）

## 错误处理

### 常见错误

**缺少 pkg 参数：**

```
ERROR: pkg parameter is required. Usage: make burn pkg=<file.binpkg> type=<full|app> mode=<usb|uart> [port=<PORT>]
```

**缺少 type 参数：**

```
ERROR: type parameter is required. Usage: make burn pkg=<file.binpkg> type=<full|app> mode=<usb|uart> [port=<PORT>]
```

**错误的文件扩展名：**

```
ERROR: pkg file must end with .binpkg, got: firmware.bin
```

**无效的 type 值：**

```
ERROR: type must be 'full' or 'app', got: partial
```

**端口检测失败：**

```
ERROR: Failed to detect Lierda QDLoader Port
Please check:
  1. Device is connected via USB
  2. Device drivers are installed
  3. Device is in download mode
```

**解决方案：** 使用 `--port COM7` 手动指定端口

## 使用提示

1. **始终使用 `.binpkg` 扩展名** - 其他扩展名将被拒绝
2. **pkg 和 type 都是必需的** - 没有自动检测或默认值
3. **推荐使用 USB 模式** - 更快且支持自动检测
4. **指定端口以提高稳定性** - 自动检测在某些环境下可能失败
5. **UART 模式需要端口** - UART 模式下端口参数是强制性的
6. **擦除行为** - 默认情况下不擦除配置和文件系统分区。使用 `--erase` 标志启用擦除。建议在以下情况下擦除：
   - 首次烧录
   - 修改分区布局后
   - 注意：校准 NV 分区永远不会被擦除，仅影响配置和文件系统分区
7. **应用程序烧录支持两种模式** - 虽然 `--type app` 需要 `.binpkg` 文件，但它只烧录应用程序分区，因此比完整烧录快得多
8. **烧录固件需要与实际硬件相一致** - 烧录前注意检查硬件型号匹配
