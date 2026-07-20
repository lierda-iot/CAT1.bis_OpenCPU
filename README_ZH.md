# OpenCPU README_zh

**[English](README.md) | 中文**

# 1 简介

OPENSDK 是 Lierda 面向 Cat.1 用户开发的一个开放型 SDK 开发系统。内核基于 FreeRTOS 开发的底包分离模式结构框架，具有完整的任务管理、内存管理、异常管理、系统时钟和中断管理。除了基础内核功能外 OPENSDK 针对 Cat.1 模组提供了丰富的外设管理、休眠唤醒、安全启动、远程升级等丰富功能，为开发者提供 "一站式" 完整软件平台，可有效降低开发门槛、缩短开发周期。

# 2 OPENSDK 框架

OPENSDK 在设计方面可以划分为三层：底包层、系统层和用户层。

```text
+----------------------------------------------------------------+
| [用户层]   app        demo        ai        ...                |
+----------------------------------------------------------------+
|      stdlibC API | EC API | LIOT API | ThreadPart API          |
+----------------------------------------------------------------+
| [系统层]                                                       |
|  +-------------------------+  +----------------------------+   |
|  | TTS  | OTA  | Driver    |  |        ThirdParty           |  |
|  +------+------+-----------+  |   lwip        CJSON         |  |
|  | SECBOOT     | PRECFG    |  |   Freertos    websocket     |  |
|  +-------------+-----------+  |   lvgl        mbedtls       |  |
|  | LIOTAPI .h  | ECAPI .h  |  |   ...                       |  |
|  +-------------+-----------+  +-----------------------------+  |
+----------------------------------------------------------------+
| [底包层]                                                       |
|  +--------+------------+------+------+--------------+          |
|  | PortLib| bootloader |  AP  |  CP  | ELF Parser   |          |
|  +--------+------------+------+------+--------------+          |
|  |     LogLib          |    MemoryMap               |          |
|  +---------------------+----------------------------+          |
|                                                                |
+----------------------------------------------------------------+
| [模组]  NT26FCNB60WNA   NT26K2B1   NT26F6D0   ...              |
+----------------------------------------------------------------+
```

1. **底包层**：底包层对于用户不可见，由 Lierda 维护，针对不同的模组型号将相关库文件打包后放在 `components/basePkg/` 目录下。底包提供了代码运行最基础的构建环境，将 SDK 所需要的 API 以及库文件进行整合，SDK 基于底包开放出来的 API 进行开发。

2. **系统层**：运行 SDK 的核心代码，系统上电后系统先完成底包与内核初始化，再加载系统层组件，从 `components/kernel/core/main.c:main()` 开始加载运行，系统层采用模块化设计包含 kernel、配置、驱动、开源第三方库、TTS、OTA 升级等，除 Kernel、配置外可以灵活裁剪。

3. **用户层**：提供给开发者创作的代码的区域，由系统层 `Main()` 函数创建 10K 大小的独立线程调用代码入口 `void user_main(void)`，用户业务代码通常从该入口开始执行。代码主要集中在 `examples` 目录下。Lierda 为开发者预设了两个工程。`app` 工程是一个空工程，没有实现具体功能，一般用户可以基于 `app` 工程上开发自己的代码。`demo` 工程是 Lierda 为客户提供的 API 接口示例的 demo，给用户参考调用。

# 3 目录结构

```
──
├── components                  （组件库，核心代码都放在这里）
│   ├── basePkg                 （底包目录，Lierda 针对不同型号生成的底包库）
│   │   └── F6B_A、F7B_A、F6D_A、K2B_A、...
│   ├── driver                  （开放的底层驱动）
│   │   └── lcd、camera、...
│   ├── kernel
│   │   ├── app.ld              （链接脚本）
│   │   ├── core                （系统启动入口）
│   │   ├── ecapi               （移芯平台相关 api 的头文件存放目录）
│   │   ├── include             （一般头文件存放目录）
│   │   └── lierda_api          （lierda 相关 api 的头文件存放目录）
│   ├── ota                     （fota 相关代码）
│   ├── precfg                  （预设相关配置代码实现，由 config/default.ini 和 iodriver.ini 传参）
│   ├── secboot                 （安全启动校验相关代码）
│   ├── thirdparty              （开源第三方的源码）
│   │   └── CJSON、freertos、websockets、lwip、mbedtls 等
│   └── tts                     （tts 相关代码）
├── config
│   ├── default.ini             （预设相关配置，如设置：默认 uartat 口、默认注网 cid、apn 等..）
│   └── iodriver.ini            （uart/spi/i2s/i2c/can 驱动的 io 引脚配置预设）
├── docs                        （相关文档）
├── examples
│   ├── app                     （默认工程，无实际功能，客户代码在此添加）
│   └── demo                    （默认示例 demo 工程，提供相关 API 接口示例）
├── LICENSE                     （开源代码许可）
├── build.bat                   （windows 下编译入口）
├── Makefile                    （Makefile 编译入口，linux 下可直接编译）
├── README_EN.md                （英文 README 文件）
├── README.md                   （中文 README 文件）
├── rules                       （Makefile 编译规则）
│   └── Makefile、Makefile.defs、Makefile.modem、Makefile.rules、Makefile.tools、Makefile.vars
└── tools                       （脚本及工具）
    └── 7z、appota、fcelf、lfsutil、msys64、precfg、secboot、toolchain
```

# 4 支持的型号

模组型号根据模组屏蔽盖上的丝印得到。底包根据芯片和功能拆分不同版本。

| 底包版本 | 芯片说明 | **FLASH** | **RAM** | **文件系统** | 适配模组型号 |
| --- | --- | --- | --- | --- | --- |
| F6B_A | EC718PM B 系列 | 812 KB | 1 MB | 780 KB | NT26FCNB30WNA / NT26FCNB60WNA |
| F6D_A | EC718PM D 系列 | 812 KB | 1 MB | 780 KB | NT26F6D0(全系列) / NT26F7D0 |
| F7B_A | EC718PM B 系列 Volte | 452 KB | 1 MB | 168 KB | NT26FCNB70WNA |
| K2B_A | EC716E B 系列 | 844 KB | 512 KB | 840 KB | NT26K2B1 / NT26K2B3 |
| K2E_A | EC716E E 系列 | 844 KB | 512 KB | 840 KB | NT26K2E0 |
| K2F_A | EC716E F 系列 | 844 KB | 512 KB | 840 KB | NT26KCNF20NNA |
| F6E_A | EC718PM F 系列 | 844 KB | 512 KB | 840 KB | NT26F6E0 (正在开发中) |
| F6C_A | EC718PM F 系列 | 812 KB | 1 MB | 780 KB | NT26F6C0 |


# 5 外设资源

| 外设 | K2B_A | F6B/F6D/F7B |
| --- | --- | --- |
| **UART** | 3 路 | 4 路 |
| | 波特率：4.9Kbps、9.6Kbps、115.2Kbps、921.6Kbps，最高可达 3 Mbps<br>数据位：5~8 位<br>奇偶校验：无校验、奇校验、偶校验<br>停止位：1、2<br>支持流量控制<br>UART1 支持 LPUART | |
| **GPIO** | 21 路 | 39 路 |
| | 包含 9 路 AONGPIO<br>电平：默认电平 1.8V，软件可调节范围：1.65V~3.30V<br>配置模式：输入、输出<br>中断触发方式：低电平、高电平、上升沿、下降沿<br>内部上拉下拉：部分 IO 支持 | |
| **I2C** | 模式：支持主、从模式<br>速率：100 KHz~1 MHz<br>寻址：支持 7/10 位寻址 | |
| **SPI** | 1 路 | 2 路 |
| | 模式：支持主、从模式<br>速率：最高支持 25.6 MHz<br>数据位宽：8 位、16 位 | |
| **USP** | 1 路 | 3 路 |
| | 数量：3 路（I2S/CSPI/LSPI）<br>模式：支持主、从模式<br>支持 8~96k、16 或 24 位，8~48k，32 位<br>支持标准 I2S/LJ/RJ 模式、PCM 模式 A/B<br>I2S/PCM 支持 TXRX/TX/RX<br>CSPI 支持从机接收<br>USP0/USP1 底层有硬件加速<br>新增支持 QSPI 与 8080 接口（仅 718(P_M/P_VM) 系列模组支持） | |
| **USB** | 支持 USB2.0<br>支持 480Mbps（HS），12Mbps（FS）数据传输<br>支持设备模式（设备） | |
| **ADC** | 2 路 | 4 路 |
| | 模拟通道：4 路，输入范围：0V~3.4V<br>温度传感器：1 路，输入范围：-40°C~85°C<br>VBAT 电压：1 路，输入范围：2.7V~4.5V<br>精度：12-bit AUXADC | |
| **Timer** | 数量：支持 6 路硬件定时器 | |
| **PWM** | 数量：支持 6 路 | |
| **低功耗定时器** | 数量：支持 7 个 | |
| **WAKEUP** | 数量：支持 6 路低功耗唤醒源 | |
| **CAN** | 不支持 | 不支持 |

# 6 快速开发

下面是快速开发指导，更详细的开发参考[《新手开发指南》](https://opendocs.lierda.com/docs/CAT.1_Doc_Protal/zh_CN/general/quick_start.html)。

## 6.1 获取模组型号和确认编译的工程

- 查看模组丝印上的模组型号。
- 工程名在 `examples/` 目录下，目录名对应工程名，如：`examples/demo` -> `demo` 工程。

## 6.2 编译

SDK 支持 Windows 和 Linux 环境开发。
Windows下可以使用 lierda_upgrade_tool 工具编译也可以使用 Makefile 编译，客户可以根据实际情况选择编译方式，具体使用方法参考：[《Lierda 蜂窝固件烧录工具使用指导》](https://opendocs.lierda.com/docs/CAT.1_Doc_Protal/zh_CN/tools/flash/flash-tool.html)

以下是主要介绍 SDK 根目录下使用命令行编译：

**6.2.1 Linux 编译**

Linux 建议系统 Ubuntu 20.04 及以上，需要安装 Make、Python3 和 32 位运行环境。

```bash
sudo apt update
sudo apt install -y make
sudo apt install -y lib32z1 lib32stdc++6
sudo apt install -y python3.10 python3.10-dev python3.10-distutils 
```

```bash
# 进入 SDK 目录
cd CAT1.bis_OpenCPU                
# 增量编译，未改动的文件不参与编译
make 
# 全清编译，清除例程后重新编译      
make all   
# 带参数的编译：全清编译 + 指定工程使用 example 下的 demo 工程 + 底包使用 F6D_A 
make all PROJECT=demo MODEM=NT26F6D0 MODEMPKG=F6D_A
```

**6.2.2 Windows 编译**

```bash
# 进入 SDK 目录
cd CAT1.bis_OpenCPU
# 增量编译，未改动的文件不参与编译
./build.bat
# 全清编译，清除例程后重新编译
./build.bat all
# 带参数的编译：全清编译 + 指定工程使用 example 下的 demo 工程 + 底包使用 F6D_A
./build.bat all PROJECT=demo MODEM=NT26F6D0 MODEMPKG=F6D_A
```

## 6.3 生成文件

编译完成后生成的文件在根目录的 **gccout/工程名** 目录下，以 demo 工程 + 模组 NT26F6D0 为例，在 **gccout/demo/** 目录下：

- `demo_NT26F6D0_01.binpkg`：底包 + 应用层的合包。
- `demo_NT26F6D0.bin`：仅包含应用层的包。
- `F6D_A_base.binpkg`：仅包含底包不包含应用的合包。

## 6.4 烧录

烧录工具我们使用 lierda_upgrade_tool 工具，工具使用文档可以参考：

[《Lierda 蜂窝固件烧录工具使用指导》](https://opendocs.lierda.com/docs/CAT.1_Doc_Protal/zh_CN/tools/flash/flash-tool.html)

## 6.5 查看日志

打开任意串口工具，波特率选择 115200，打开串口后，即可查看 APP 的 log。

更多日志详情参考：[《Windows Log 抓取指南》](https://opendocs.lierda.com/docs/CAT.1_Doc_Protal/zh_CN/tools/log/log-tool.html)

# 7 文档

[通用指南](https://opendocs.lierda.com/docs/CAT.1_Doc_Protal/zh_CN/general/index.html) | [硬件资料](https://opendocs.lierda.com/docs/CAT.1_Doc_Protal/zh_CN/hardware/index.html) | [软件开发](https://opendocs.lierda.com/docs/CAT.1_Doc_Protal/zh_CN/software/index.html) | [工具](https://opendocs.lierda.com/docs/CAT.1_Doc_Protal/zh_CN/tools/index.html)

# 8 更多开发示例

[《基于L-CT4IT00-YP00W-01_V04硬件》](https://opendocs.lierda.com/docs/CAT.1_Doc_Protal/zh_CN/examples/L-CT4IT00-YP00W-01_V04/L-CT4IT00-YP00W-01_V04_quick_start.html)

[ 《Lierda 小达 应用指导》](https://opendocs.lierda.com/docs/CAT.1_Doc_Protal/zh_CN/examples/XiaoDa/XiaoDa%20Application%20Guide_Rev0.1.html)
# 9 工具

- [《USB驱动安装指导》](https://opendocs.lierda.com/docs/CAT.1_Doc_Protal/zh_CN/tools/usb/USB_Driver_Installation_Guide.html)

# 10 许可协议

71x OPENSDK 遵循 Apache License 2.0 开源许可协议，可以免费在商业产品中使用，并且不需要公开私有代码，没有潜在商业风险。

```c
/*
 * Copyright (c) Lierda Science & Technology Group Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
```
