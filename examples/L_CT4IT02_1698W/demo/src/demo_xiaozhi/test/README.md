# demo_xiaozhi 核心逻辑单元测试

## 作用

`demo_xiaozhi`（小智 AI 语音 demo）包含多处纯字符串 / 哈希逻辑，其中两处高价值点被抽到
header-only 的 [`demo_xiaozhi_core.h`](../src/demo_xiaozhi_core.h)：

- WebSocket URL 解析（`wss://host[:port]/path`，`xiaozhi_ws.c` 的 `xz_ws_parse_url`）；
- FNV-1a 32 位哈希（`xiaozhi_identity.c` 的 `xz_hash`，用于 IMEI 派生设备 ID）。

生产 `.c` 中的实现改为调用 core 内联函数，行为不变；测试在主机端直接验证，不需要 liot
RTOS、SSL 或 WebSocket 连接。

## 如何触发

单独运行：

```sh
make -C examples/L_CT4IT02_1698W/demo/src/demo_xiaozhi/test test
```

通过总入口运行（模块名 `xiaozhi`）：

```sh
make -C examples/L_CT4IT02_1698W/tests host-test MODULE=xiaozhi
```

冒烟门（模块内单用例快捷校验）：

```sh
make -C examples/L_CT4IT02_1698W/tests host-test-smoke MODULE=xiaozhi
```

> 注：Unity 当前 `-n` 参数可解析但不跳过执行；只看单个用例请配合
> `grep '<用例名>.*PASS'` 使用。

查看用例清单 / 清理产物：

```sh
make -C examples/L_CT4IT02_1698W/demo/src/demo_xiaozhi/test list-tests
make -C examples/L_CT4IT02_1698W/demo/src/demo_xiaozhi/test clean
```

## 用例与断言

### test_parse_ws_url_host_port_path

`wss://api.example.com:8443/v1/voice` 解析出 host、路径 `/v1/voice` 与端口 8443。

### test_parse_ws_url_default_port_and_root

无端口时默认 443；无路径时输出 `/`（`wss://host` 与 `wss://host/` 均验证）。

### test_parse_ws_url_invalid_and_truncation

`NULL`、非 `wss://` 前缀返回 `-1`；空主机与 host 缓冲过小返回 `-2`（与生产代码的
-1/-2 错误码约定一致，生产实现先做 host 长度检查再校验指针）。

### test_fnv1a_known_vector_and_seed

标准 FNV-1a 校验值：空串 → `0x811C9DC5`、`"hello"` → `0x4F9F2CAB`；不同 seed 结果不同；
`NULL` 文本直接返回 seed（生产新增的防御路径）。
