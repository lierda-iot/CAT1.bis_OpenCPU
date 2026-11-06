# 多 APP 加载器使用说明

## 概述

多 APP 方案允许在同一个 FLASH 中编译和烧录多个独立的应用程序（APP0 和 APP1），并通过 APP0 手动加载 APP1。

**注意：** 此功能默认禁用，需要通过 `BUILD_COMP_MULTIAPP_EN=y` 显式启用。

## 特性

- **APP0（主应用）**：包含多 APP 加载器代码，可以加载并启动 APP1
- **APP1（子应用）**：独立编译的应用程序，由 APP0 手动加载
- **独立 RAM 分配**：APP0 和 APP1 各自使用独立的 RAM 区域
- **手动加载**：客户在 `user_main` 中调用 `Liot_MultiAppLoad` 加载 APP1
- **可选功能**：通过 `BUILD_COMP_MULTIAPP_EN` 开关控制是否启用

---

## 功能开关控制

### BUILD_COMP_MULTIAPP_EN 变量

`BUILD_COMP_MULTIAPP_EN` 是一个 Makefile 变量，用于控制是否启用多 APP 加载器功能。

#### 配置位置

在 `rules/Makefile.defs:99` 定义：
```makefile
BUILD_COMP_MULTIAPP_EN ?= n
```

#### 1. 禁用多 APP 功能（默认）

```bash
make -j4 APP_SLOT=0 BUILD_COMP_MULTIAPP_EN=n
```

**效果：**
- ✅ 不编译 `liot_multiapp.c` 加载器代码
- ✅ FLASH 使用率更低（2.77% vs 3.01%）
- ✅ 不需要配置 `MULTIAPP_SLOT1_FLASH_SIZE` 和 `MULTIAPP_SLOT1_RAM_SIZE`
- ✅ 内存布局更简单（只有 APP0）
- ✅ 默认配置，适合单 APP 应用

#### 2. 启用多 APP 功能

```bash
make -j4 APP_SLOT=0 \
    BUILD_COMP_MULTIAPP_EN=y \
    MULTIAPP_SLOT1_FLASH_SIZE=0x10000 \
    MULTIAPP_SLOT1_RAM_SIZE=0x4000
```

**效果：**
- ✅ 编译 `liot_multiapp.c` 加载器代码
- ✅ 支持多 APP 加载器
- ✅ FLASH 和 RAM 会预留 APP1 的空间
- ✅ 提供 `Liot_MultiAppLoad()` API
- ✅ 适合需要动态加载子应用的场景

### 功能开关的影响

| 配置 | `BUILD_COMP_MULTIAPP_EN=n` | `BUILD_COMP_MULTIAPP_EN=y` |
|------|----------------------------|----------------------------|
| **编译 `liot_multiapp.c`** | ❌ 不编译 | ✅ 编译进 APP0 |
| **`Liot_MultiAppLoad()` API** | ❌ 不可用 | ✅ 可用 |
| **内存布局** | 单 APP（只有 APP0） | 多 APP（APP0 + APP1） |
| **FLASH 预留** | 无 APP1 空间 | 预留 `MULTIAPP_SLOT1_FLASH_SIZE` |
| **RAM 预留** | 无 APP1 空间 | 预留 `MULTIAPP_SLOT1_RAM_SIZE` |
| **FLASH 使用率** | 更低（~2.77%） | 更高（~3.01%） |
| **对齐检查** | ❌ 不检查 | ✅ 检查 4KB FLASH / 4 字节 RAM 对齐 |
| **适用场景** | 简单单 APP 应用 | 复杂多 APP 应用 |

### 技术实现

#### 1. Makefile 控制

在 `components/multiapp/Makefile` 中：

```makefile
ifeq ($(BUILD_COMP_MULTIAPP_EN),y)
    # 启用多 APP 功能
    ifeq ($(APP_SLOT),0)
        obj-y += components/multiapp/liot_multiapp.o
    endif
endif
```

**逻辑：**
- `BUILD_COMP_MULTIAPP_EN=y` 时：
  - 仅在编译 APP0 时编译 `liot_multiapp.c`
  - APP1 不需要编译加载器代码
- `BUILD_COMP_MULTIAPP_EN=n` 时：
  - 不编译任何多 APP 相关代码

#### 2. 内存布局自动调整

在 `components/kernel/include/mem_map.h` 中：

```c
// APP0 的 FLASH 大小会自动减去 APP1 空间
#define CODE_FALSH_SIZE (CONFIG_APP_BURN_SIZE - CONFIG_SECBOOT_HEAD_LENGTH \
                         - FLASH_TTS_REGION_SIZE + FLASH_REGION_ADJUST \
                         - MULTIAPP_SLOTN_FLASH_SIZE_SUM)
```

- `BUILD_COMP_MULTIAPP_EN=y` 时：`MULTIAPP_SLOTN_FLASH_SIZE_SUM = MULTIAPP_SLOT1_FLASH_SIZE`
- `BUILD_COMP_MULTIAPP_EN=n` 时：`MULTIAPP_SLOTN_FLASH_SIZE_SUM = 0x00`

### 对齐检查

当 `BUILD_COMP_MULTIAPP_EN=y` 时，Makefile 会自动检查：

```makefile
# FLASH 必须 4KB (0x1000) 对齐
ifneq ($(MULTIAPP_SLOT1_FLASH_SIZE),0x00)
    ifeq ($(shell printf '%d' $$(($(MULTIAPP_SLOT1_FLASH_SIZE) & 0xFFF))),0)
    else
        $(error MULTIAPP_SLOT1_FLASH_SIZE=$(MULTIAPP_SLOT1_FLASH_SIZE) 未 4k 对齐)
    endif
endif

# RAM 必须 4 字节对齐
ifneq ($(MULTIAPP_SLOT1_RAM_SIZE),0x00)
    ifeq ($(shell printf '%d' $$(($(MULTIAPP_SLOT1_RAM_SIZE) & 0x3))),0)
    else
        $(error MULTIAPP_SLOT1_RAM_SIZE=$(MULTIAPP_SLOT1_RAM_SIZE) 未 4 字节对齐)
    endif
endif
```

**为什么要对齐？**
- **FLASH 4KB 对齐**：FLASH 擦除粒度是 4KB，不对齐会导致擦除失败
- **RAM 4 字节对齐**：ARM Cortex-M 要求 RAM 地址 4 字节对齐，不对齐会导致 Hard Fault

### 编译结果对比

#### 禁用多 APP（BUILD_COMP_MULTIAPP_EN=n）

```
Memory region         Used Size  Region Size  %age Used
           FLASH:       23052 B       812 KB      2.77%
             RAM:         492 B         1 MB      0.05%
```

- APP0 拥有全部 FLASH (812 KB)
- APP0 拥有全部 RAM (1 MB)
- 不包含 multiapp 加载器代码

#### 启用多 APP（BUILD_COMP_MULTIAPP_EN=y）

```
Memory region         Used Size  Region Size  %age Used
           FLASH:       23052 B       748 KB      3.01%
             RAM:         492 B      1008 KB      0.05%
```

- APP0 拥有 748 KB FLASH (812KB - 64KB)
- APP0 拥有 1008 KB RAM (1MB - 16KB)
- APP1 拥有 64 KB FLASH
- APP1 拥有 16 KB RAM
- 包含 multiapp 加载器代码（约 0.24% FLASH 增加）

---

## 配置参数

在顶层 `Makefile` 或编译命令行中配置：

```makefile
# 多 APP 功能开关（y=启用，n=禁用，默认 n）
BUILD_COMP_MULTIAPP_EN ?= n

# 当前编译的 APP slot（0=APP0，1=APP1）
APP_SLOT ?= 0

# APP1 FLASH 大小（仅在 BUILD_COMP_MULTIAPP_EN=y 时有效）
MULTIAPP_SLOT1_FLASH_SIZE ?= 0x10000  # 64KB

# APP1 RAM 大小（仅在 BUILD_COMP_MULTIAPP_EN=y 时有效）
MULTIAPP_SLOT1_RAM_SIZE ?= 0x4000     # 16KB
```

### 对齐要求

- **FLASH 大小**：必须 **4KB (0x1000) 对齐**（FLASH 擦除粒度）
- **RAM 大小**：必须 **4 字节对齐**

---

## 编译步骤

### 方案 1：单 APP 模式（默认，推荐用于简单项目）

如果你的项目只需要一个 APP，保持 `BUILD_COMP_MULTIAPP_EN=n`（默认值）：

```bash
# 编译单 APP 项目
make clean
make -j4 APP_SLOT=0
```

**优点：**
- ✅ FLASH 利用率最高
- ✅ 配置最简单
- ✅ 编译最快
- ✅ 不需要配置 MULTIAPP_SLOT1_FLASH_SIZE

---

### 方案 2：多 APP 模式（用于复杂项目）

如果你需要加载多个 APP，设置 `BUILD_COMP_MULTIAPP_EN=y`：

#### 1. 编译 APP0（主应用）

```bash
make clean
make -j4 APP_SLOT=0 \
    BUILD_COMP_MULTIAPP_EN=y \
    MULTIAPP_SLOT1_FLASH_SIZE=0x10000 \
    MULTIAPP_SLOT1_RAM_SIZE=0x4000
```

生成文件：
- `gccout/app/app_NT26FCNB60WNA.bin` - APP0 固件
- `gccout/app/app_NT26FCNB60WNA_01.binpkg` - APP0 烧录包

#### 2. 编译 APP1（子应用）

```bash
make clean
make -j4 APP_SLOT=1 \
    BUILD_COMP_MULTIAPP_EN=y \
    MULTIAPP_SLOT1_FLASH_SIZE=0x10000 \
    MULTIAPP_SLOT1_RAM_SIZE=0x4000
```

生成文件：
- `gccout/app/app_NT26FCNB60WNA.bin` - APP1 固件

#### 3. 将 APP1 打包进 APP0

**方法 1：通过 `SUBAPP_BIN` 参数自动打包（推荐）**

```bash
# 先编译 APP1 并保存到临时目录
make clean
make -j4 APP_SLOT=1 \
    BUILD_COMP_MULTIAPP_EN=y \
    MULTIAPP_SLOT1_FLASH_SIZE=0x10000 \
    MULTIAPP_SLOT1_RAM_SIZE=0x4000
cp gccout/app/app_NT26FCNB60WNA.bin /tmp/app1.bin

# 再编译 APP0，自动打包 APP1
make clean
make -j4 APP_SLOT=0 \
    BUILD_COMP_MULTIAPP_EN=y \
    MULTIAPP_SLOT1_FLASH_SIZE=0x10000 \
    MULTIAPP_SLOT1_RAM_SIZE=0x4000 \
    SUBAPP_BIN=/tmp/app1.bin
```

这会将 APP1 的 bin 文件自动打包进 APP0 的 binpkg。

**方法 2：手动使用 fcelf 工具打包**

```bash
# 1. 先编译 APP0
make clean
make -j4 APP_SLOT=0 \
    BUILD_COMP_MULTIAPP_EN=y \
    MULTIAPP_SLOT1_FLASH_SIZE=0x10000 \
    MULTIAPP_SLOT1_RAM_SIZE=0x4000

# 2. 再编译 APP1，保存 bin 文件
make clean
make -j4 APP_SLOT=1 \
    BUILD_COMP_MULTIAPP_EN=y \
    MULTIAPP_SLOT1_FLASH_SIZE=0x10000 \
    MULTIAPP_SLOT1_RAM_SIZE=0x4000
cp gccout/app/app_NT26FCNB60WNA.bin gccout/app/app1.bin

# 3. 重新打包 APP0，包含 APP1
cd gccout/app
tools/flashtools/fcelf -M \
    -input basepkg/bootloader.bin -addrname BL_PKGIMG_LNA -flashsize BOOTLOADER_PKGIMG_LIMIT_SIZE \
    -input basepkg/ap_lierda_app.bin -addrname AP_PKGIMG_LNA -flashsize AP_PKGIMG_LIMIT_SIZE \
    -input basepkg/cp-demo-flash.bin -addrname CP_PKGIMG_LNA -flashsize CP_PKGIMG_LIMIT_SIZE \
    -input app_NT26FCNB60WNA.bin -addrname PKGFLXAPP_APP0_LNA -flashsize PKGFLXAPP_APP0_SIZE \
    -input app1.bin -addrname PKGFLXAPP_APP1_LNA -flashsize PKGFLXAPP_APP1_SIZE \
    -pkgmode 1 \
    -productname <PRODUCT_NAME> \
    -def mem_map.txt \
    -outfile app_with_app1.binpkg
```

---

## 使用方法

### API 接口

```c
/**
 * @brief  加载并启动指定地址的 APP
 * @param  app_addr      APP 的 FLASH 起始地址
 * @param  arg1..arg4    传递给 APP main 函数的 4 个参数
 * @return 0 成功启动；-1 APP 头校验失败
 * @warning 必须在 SysTabInit() 成功之后执行
 */
int Liot_MultiAppLoad(uint32_t app_addr, uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4);
```

### 示例代码

在 `examples/app/src/user_main.c` 中：

```c
#include "liot_multiapp.h"
#include "mem_map.h"

void user_main(void)
{
    printf("APP0 started\n");
    
    // 加载并启动 APP1
    int ret = Liot_MultiAppLoad(PKGFLXAPP_APP1_LNA, 0, 0, 0, 0);
    if (ret == 0) {
        printf("APP1 loaded successfully\n");
    } else {
        printf("APP1 load failed\n");
    }
    
    // APP0 的其他逻辑
    // ...
}
```

---

## 内存布局

### FLASH 布局

```
+---------------------+
|   Bootloader        |
|   AP (底包)         |
|   CP (底包)         |
+---------------------+
|   APP0 (主应用)     | ← PKGFLXAPP_APP0_LNA
|   可变大小          |   大小：总空间 - APP1_SIZE
+---------------------+
|   APP1 (子应用)     | ← PKGFLXAPP_APP1_LNA
|   固定大小          |   大小：MULTIAPP_SLOT1_FLASH_SIZE
+---------------------+
|   TTS 资源区        |
+---------------------+
```

### RAM 布局

```
+---------------------+
|   APP0 RAM          | ← PKGFLXAPP_APP0_RAM_ADDR
|   可变大小          |   大小：总 RAM - APP1_RAM_SIZE
+---------------------+
|   APP1 RAM          | ← PKGFLXAPP_APP1_RAM_ADDR
|   固定大小          |   大小：MULTIAPP_SLOT1_RAM_SIZE
+---------------------+
```

---

## 注意事项

1. **FLASH 对齐**：`MULTIAPP_SLOT1_FLASH_SIZE` 必须 4KB 对齐，否则编译时报错
2. **RAM 对齐**：`MULTIAPP_SLOT1_RAM_SIZE` 必须 4 字节对齐，否则编译时报错
3. **APP1 大小限制**：APP1 的实际大小不能超过 `MULTIAPP_SLOT1_FLASH_SIZE`
4. **RAM 大小限制**：APP1 的实际 RAM 使用不能超过 `MULTIAPP_SLOT1_RAM_SIZE`
5. **APP1 地址**：通过 `mem_map.h` 中的 `PKGFLXAPP_APP1_LNA` 宏获取 APP1 的加载地址
6. **手动加载**：APP1 不会自动启动，必须在 APP0 的 `user_main` 中手动调用 `Liot_MultiAppLoad`
7. **功能开关**：必须设置 `BUILD_COMP_MULTIAPP_EN=y` 才能使用多 APP 功能

---

## 配置示例

### 小型配置（测试用）
```bash
BUILD_COMP_MULTIAPP_EN=y
MULTIAPP_SLOT1_FLASH_SIZE=0x10000  # 64KB
MULTIAPP_SLOT1_RAM_SIZE=0x4000     # 16KB
```

### 中型配置
```bash
BUILD_COMP_MULTIAPP_EN=y
MULTIAPP_SLOT1_FLASH_SIZE=0x40000  # 256KB
MULTIAPP_SLOT1_RAM_SIZE=0x20000    # 128KB
```

### 大型配置
```bash
BUILD_COMP_MULTIAPP_EN=y
MULTIAPP_SLOT1_FLASH_SIZE=0x80000  # 512KB
MULTIAPP_SLOT1_RAM_SIZE=0x40000    # 256KB
```

---

## 烧录

使用生成的 binpkg 文件烧录：

```bash
make burn pkg=gccout/app/app_NT26FCNB60WNA_01.binpkg type=full mode=usb
```

---

## 常见问题

### Q: 为什么启用多 APP 后 FLASH 使用率变高了？

A: 因为 APP0 的可用 FLASH 减少了（预留给 APP1），使用率 = 实际使用 / 可用空间。
   - 禁用：23052 B / 812 KB = 2.77%
   - 启用：23052 B / 748 KB = 3.01%
   - 实际 APP0 大小不变，只是分母变小了

### Q: 可以只编译 APP0 不编译 APP1 吗？

A: 可以。设置 `BUILD_COMP_MULTIAPP_EN=y` 后，即使不编译 APP1，APP0 也能正常运行。APP1 的空间会被预留但不使用。

### Q: 可以在运行时决定是否加载 APP1 吗？

A: 可以。`Liot_MultiAppLoad()` 是手动调用的，客户可以在 `user_main()` 中根据条件决定是否加载。

### Q: BUILD_COMP_MULTIAPP_EN=n 时可以使用 Liot_MultiAppLoad() 吗？

A: 不可以。`BUILD_COMP_MULTIAPP_EN=n` 时不会编译 `liot_multiapp.c`，调用该函数会导致链接错误。

### Q: 如何验证 APP1 是否被打包进 binpkg？

A: 使用 fcelf 工具解析 binpkg：
```bash
tools/flashtools/fcelf -U -productname <PRODUCT_NAME> \
    -input gccout/app/app_NT26FCNB60WNA_01.binpkg \
    -def gccout/app/mem_map.txt
```
应该能看到 `app1.bin` 文件的条目。

---

## 文件说明

- `liot_multiapp.h` - 多 APP 加载器头文件
- `liot_multiapp.c` - 多 APP 加载器实现（仅编入 APP0）
- `mem_map.h` - 内存布局定义（包含 APP0/APP1 的 FLASH 和 RAM 地址）
- `Makefile` - 多 APP 构建规则
- `BUILD_MULTIAPP_CONTROL.md` - 功能开关详细说明（已合并到本文档）

---

## 扩展

如果需要支持更多 APP（APP2、APP3 等），需要：
1. 在 `Makefile` 中添加 `MULTIAPP_SLOT2_FLASH_SIZE/RAM_SIZE` 等参数
2. 在 `mem_map.h` 中添加相应的地址定义
3. 调整 `MULTIAPP_SLOTN_FLASH_SIZE_SUM` 和 `MULTIAPP_SLOTN_RAM_SIZE_SUM` 计算

目前设计简化为只支持 2 个 slot（APP0 + APP1）。

---

## 总结

- **`BUILD_COMP_MULTIAPP_EN=n`**：单 APP 模式（默认，推荐用于简单项目）
- **`BUILD_COMP_MULTIAPP_EN=y`**：多 APP 模式（需要配置 FLASH/RAM 大小，用于复杂项目）
- 功能开关完全独立，可以在项目初期选择，后期也可以切换
- 对齐检查确保配置正确，避免运行时错误
- 支持 `SUBAPP_BIN` 参数自动打包 APP1，简化构建流程
