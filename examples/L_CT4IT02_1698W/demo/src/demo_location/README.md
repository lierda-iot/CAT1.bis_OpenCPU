# L_CT4IT02_1698W 定位（Location）Demo 说明

更新时间：2026-10-09

## 1. 示例文件

- 源码目录：`examples/L_CT4IT02_1698W/demo/src/demo_location/`
- 核心逻辑：`demo_location/location_core_utils.{c,h}`（e7 坐标格式化）
- 入口函数：`liot_location_demo_thread(void *argv)`

## 2. 示例用途

该 Demo 用于验证 CC1161W UART GNSS 接收链路与逆地理编码（reverse-geocode）链路：

- 蜂窝网络注册与数据通路（SIM 0、PDP CID 1）
- AGNSS 辅助数据下载与注入，注入后等待 60s 定位（超时上报 `-41`）
- 坐标显示与 `Locate` 按钮触发 HTTPS GET 逆地理编码
- 解析服务端 `success` + `data.displayName` 并显示

## 3. 当前固定配置

- 逆地理编码服务：`https://ai.iwg.senthink.com/api/reverse-geocode`
- 请求参数：`longitude` / `latitude`（e7 定点数，度 × 10,000,000）
- 网络：SIM 0、PDP CID 1、HTTPS 且本 Demo 关闭证书校验
- AGNSS 服务：`http://icoe-lierd-api.rx-networks.cn/rxn-api/locationApi/rtcm`（明文 HTTP 80）
- AGNSS 账号：`cId=xKmat46WVf`、`mId=NT26KCNE20GNA`、`pw=VmVwQjltUUh0ZlhuM000Wg==`
- AGNSS 结果码：`-30` 默认未走通；`-31` DNS 失败；`-32` connect 失败；`-33` 请求报文构造失败；`-34` 发送不完整；`-35` 内存分配失败；`-36` 收 HTTP header 失败；`-37` HTTP 非 200 或 Content-Length 非法/超限；`-38` 收 body 不完整；`-39` RTCM 长度 <4 或首字节非 `0xD3`；`-40` GNSS UART 未就绪；`-41` 注入成功但 60s 无 fix；`-42` `HWDEMO_GNSS_EN` 未使能
- AGNSS fix 等待：一次性等待 `AGNSS_FIX_TIMEOUT_MS=60000`，超时返回 `-41`（当前无自动重试）
- 坐标来源：
  - `HWDEMO_GNSS_EN=y` 时通过 `demo_gnss_get_location()` 读取 GNSS 定位
  - 未使能 GNSS 时使用源码内 mock 坐标 `302741500 / 1201551000`（即 30.2741500 N / 120.1551000 E）
- UI 模式（默认）：`lvgl_init()` + `demo_location_page_enter_async()`，页面按钮 `Locate` 触发请求
- Headless 模式（`HWDEMO_LOCATION_HEADLESS_EN=y`）：无 LVGL，按键 `KEY_USER0` 触发请求，并输出 mock 坐标与网络就绪日志

## 4. 编译开关

- `HWDEMO_LOCATION_EN=y`：编译 location demo，并自动拉起 LVGL / GSENSOR 依赖
- `HWDEMO_GNSS_EN=y`：使用真实 GNSS 定位（不使能时走 mock 坐标）
- `HWDEMO_LOCATION_HEADLESS_EN=y`：无 UI 模式（默认 n）
- `HWDEMO_SOUND_ES8375_EN` 默认 n：板载声音 demo 三选一默认关闭，无需显式设置。

## 5. 建议编译方式

demo 模式单独验证（使用 GNSS 真机定位）：

```bash
make all PROJECT=L_CT4IT02_1698W MODEM=NT26F9D0 MODEMPKG=F9D_A BUILD_MODE=demo HWDEMO_GNSS_EN=y HWDEMO_LOCATION_EN=y
```

仅验证逆地理编码链路（无 GNSS 模块，使用 mock 坐标）：

```bash
make all PROJECT=L_CT4IT02_1698W MODEM=NT26F9D0 MODEMPKG=F9D_A BUILD_MODE=demo HWDEMO_LOCATION_EN=y
```

Headless 模式（无 LVGL、按键触发）：

```bash
make all PROJECT=L_CT4IT02_1698W MODEM=NT26F9D0 MODEMPKG=F9D_A BUILD_MODE=demo HWDEMO_GNSS_EN=y HWDEMO_LOCATION_EN=y HWDEMO_LOCATION_HEADLESS_EN=y
```

## 6. 运行方式

启用后系统启动自动创建 `liot_location_demo_thread`，串口 115200。GNSS 模组天线朝上放置，室外或窗边等待定位。

## 7. 预期结果

### 7.1 预期日志前缀

```text
[location]
```

### 7.2 预期关键日志

UI 模式与 headless 模式共同的启动序列：

```text
[location] standalone demo start
[location] SDK network register wait start
[location] SDK network registered attempts=...
[location] boot network result=0
[location] AGNSS start after network ready
```

使能 GNSS 时，AGNSS 下载/注入/等待序列（成功样例，真机实测）：

```text
[location] AGNSS socket DNS ret=0 addresses=...
[location] AGNSS socket connect fd=0 ret=0 errno=0
[location] AGNSS socket request sent=339/339
[location] AGNSS socket HTTP status=200 content_length=4664 initial_body=...
[location] AGNSS socket RTCM bytes=4664 head=D3 00 3D 3F
[gnss-cc1161w] AGNSS UART injected=4664/4664
[gnss-cc1161w] AGNSS injection complete tick=..., waiting for valid fix
[gnss-cc1161w] AIDINFO query sent=10/10
[gnss-cc1161w] AIDINFO response=$AIDINFO,0x00FFFFFFFF,...
[location] AGNSS socket result=0
```

60s 内未获得定位（室内/无天线典型场景）时：

```text
[location] AGNSS no fix after 60000 ms sats=0 result=0
[location] AGNSS socket result=-41
[location] AGNSS result=-41 fix=0 sats=0
```

Headless 模式额外输出：

```text
[location] mock GNSS lat_e7=302741500 lon_e7=1201551000
[location] network ready; press KEY_USER0 for GET
[location] key pressed; GET begin
[location] GET result=0 location=...
```

UI 模式页面等待定位成功后显示坐标；点击 `Locate` 后请求逆地理编码服务，成功时页面显示服务端返回的 `displayName`。

## 8. 验证状态

- 编译已验证（demo 组合，`HWDEMO_LOCATION_EN=y`，GNSS 开/关两种组合）。
- 纯逻辑（e7 坐标格式化、紧凑格式、边界截断）由单元测试覆盖，见 [test/README.md](test/README.md)。
- 真机实测（室内两次）：网络注册成功 → AGNSS HTTP `200`、RTCM `4664` 字节完整注入 UART → AIDINFO 查询/响应正常 → 60s 内始终 `GGA fix=0 sats=00`、`GSV visible_sats=0`，最终上报 `-41`（注入成功但无 fix）。
- 结论：AGNSS 数据通路与注入链路正常，无 fix 根因在物理层（室内办公环境、天线遮挡/信号弱），AGNSS 无法凭空产生卫星信号；该 Demo 侧无需代码修复。

## 9. 当前边界

- 网络传输需支持页面退出时的取消/请求代次检查；页面退出时若请求仍在进行，不得在任务中直接更新 LVGL 对象，所有 UI 更新由 100ms UI 定时器拥有。
- 逆地理编码依赖云端服务可用性与 SIM 数据业务。
- AGNSS fix 为 60s 一次性等待，超时返回 `-41` 后不自动重试，后续可手动重新触发或重启 Demo 重试。
- 室内/无天线环境下 `sats=00`、`fix=0` 属物理层现象，不是软件缺陷。
- 本 Demo 只验证定位与逆地理编码，不含地图展示（地图在 app 模式）。
