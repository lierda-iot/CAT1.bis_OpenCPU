# liot_ai 组件

## 概述

`liot_ai` 是一个面向 AI 语音交互场景的**事件驱动应用框架**，运行在 FreeRTOS 之上。它提供了一套 Policy-Job 架构，用于管理设备的业务状态（如语音唤醒对话、开关机流程），并通过事件队列解耦硬件模块（按键、音频、LED）与业务逻辑。

## 目录结构

```
liot_ai/
├── main.c                          # 入口任务 liot_ai_task()
├── Makefile.inc                    # 构建配置
├── framework/                      # 核心框架层
│   ├── frameworkTypes.h            # 公共类型定义（事件、Job、Policy）
│   ├── frameworkCore.h/.c          # 框架核心：事件循环、Job 生命周期、Policy 调度
│   └── frameworkTimer.h/.c         # 软件定时器管理
├── projects/ai_demo/              # 示例应用
│   ├── app/
│   │   ├── appFramework.h/.c      # 应用初始化入口
│   │   ├── globalPolicy.c         # 全局 Policy（处理开关机按键）
│   │   ├── globalPrivate.h
│   │   ├── powerJob.c             # 开机 Job 实现
│   │   ├── powerPolicy.c          # 开机 Job 专属 Policy
│   │   ├── powerPrivate.h
│   │   ├── talkJob.c              # 语音对话 Job 实现
│   │   ├── talkPolicy.c           # 对话 Job 专属 Policy
│   │   └── talkPrivate.h
│   └── modules/
│       ├── audioModule.h/.c        # 音频模块（播放、采集、音量）
│       ├── keyModule.h/.c          # 按键模块（开关机键事件）
│       └── ledModule.h/.c          # LED 灯效模块
└── utils/
    └── aiLog.h/.c                  # 日志工具（分级：DEBUG/INFO/WARN/ERROR）
```

## 核心架构

### Policy-Job 模型

框架采用**两级事件分发**机制：

```
硬件中断/模块 → Event Queue → Framework Task → Policy 匹配 → Job 执行
```

1. **Event（事件）**：所有输入（按键、定时器到期、音频完成等）统一封装为 `event_t` 投递到事件队列
2. **Policy（策略）**：决策层，负责判断事件应如何处理（启动 Job、停止 Job、转发给当前 Job、延迟等）
3. **Job（任务）**：业务执行单元，具有 `onStart/onEvent/onStop` 生命周期回调

### 事件处理优先级

```
1. 当前 Job 的专属 Policy（jobPolicy）
2. 全局 Policy 列表（按注册顺序）
3. 未匹配则丢弃并打印警告
```

### 系统状态

| 状态 | 含义 |
|------|------|
| `SYS_STATE_IDLE` | 空闲，无活跃 Job |
| `SYS_STATE_ACTIVE` | 活动态，存在正在执行的 Job |

### Policy 决策结果

| 结果 | 行为 |
|------|------|
| `POLICY_IGNORE` | 忽略事件 |
| `POLICY_START_JOB` | 启动指定类型的新 Job |
| `POLICY_STOP_JOB` | 停止当前 Job |
| `POLICY_POST_CURR_JOB` | 将事件转发给当前 Job 的 onEvent |
| `POLICY_ABORT_CURR_AND_START_NEW` | 中止当前 Job 并启动新 Job |
| `POLICY_DELAY` | 延迟事件，等当前 Job 结束后再处理 |
| `POLICY_DIRECT_HANDLE` | 直接执行处理函数，不经过 Job |

## 框架 API

### 初始化与启动

```c
bool frameworkInit(void);    // 初始化事件队列、定时器
bool frameworkStart(void);   // 创建框架任务，开始事件循环
```

### 注册

```c
bool frameworkRegisterPolicy(policy_t *policy);     // 注册全局 Policy
bool frameworkRegisterJob(const jobDesc_t *jobDesc); // 注册 Job 类型
```

### 事件投递

```c
bool frameworkPostEvent(const event_t *event);  // 投递事件（支持 ISR 上下文）
```

### Job 控制

```c
bool frameworkStartJob(jobType_E jobType, const event_t *triggerEvent);
void frameworkStopCurrentJob(jobStopReason_E reason);
const job_t *frameworkGetCurrentJob(void);
```

### 定时器

```c
bool frameworkTimerStart(timerId_E timerId, uint32_t timeoutMs);
bool frameworkTimerStop(timerId_E timerId);
void frameworkTimerStopAll(void);
```

## 示例应用 (ai_demo)

### 已实现的 Job

| Job 类型 | 文件 | 功能 |
|----------|------|------|
| `JOB_TYPE_POWER` | powerJob.c | 开关机流程处理 |
| `JOB_TYPE_TALK` | talkJob.c | 语音唤醒对话（状态机：提示音→聆听→待机） |

### 硬件模块

| 模块 | 功能 |
|------|------|
| `keyModule` | 按键检测，产生 `EVT_KEY_POWER_ON/OFF` 事件 |
| `audioModule` | 音频播放、采集（VAD）、音量控制 |
| `ledModule` | LED 灯效控制（熄灭、交互闪烁等） |

### 典型流程

```
用户按下电源键
  → keyModule 产生 EVT_KEY_POWER_ON 事件
  → frameworkPostEvent() 投递到队列
  → frameworkTask 取出事件
  → globalPolicy.match() 匹配成功
  → globalPolicy.decide() 返回 POLICY_START_JOB, nextJobType=JOB_TYPE_POWER
  → frameworkStartJob() 创建 powerJob
  → powerJobOnStart() 执行开机逻辑（播放提示音、LED 闪烁等）
```

## 如何扩展

### 添加新 Job

1. 定义 `jobType_E` 新枚举值
2. 实现 `onStart/onEvent/onStop` 回调
3. 创建 `jobDesc_t` 描述符并通过 `frameworkRegisterJob()` 注册
4. 可选：为该 Job 创建专属 Policy

### 添加新 Policy

1. 实现 `match()` 函数（粗筛事件）
2. 实现 `decide()` 函数（返回决策结果和动作）
3. 通过 `frameworkRegisterPolicy()` 注册为全局 Policy

### 添加新事件

1. 在 `eventId_E` 中添加新枚举值
2. 在相应模块中通过 `frameworkPostEvent()` 投递
3. 在对应 Policy 或 Job 中处理

## 设计特点

- **单 Job 模型**：同一时刻只有一个活跃 Job，简化状态管理
- **延迟队列**：当前 Job 未结束时，新事件可暂存，Job 结束后自动重放
- **ISR 安全**：`frameworkPostEvent()` 自动检测中断上下文，使用对应的 FreeRTOS API
- **解耦设计**：硬件模块只负责产生事件，不直接调用业务逻辑
- **静态注册**：Job 和 Policy 在初始化阶段注册，运行时无动态分配
