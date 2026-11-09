# mp3 用例功能与使用说明

- 模块目录：`demo/src/demo_mp3/test`
- 被测 seam：`demo/src/demo_mp3/demo_mp3_core.{c,h}`（与 `demo_mp3_stream.c` 生产实现同一份源码）
- 用例总数：6
- 运行全部：`make -C examples/L_CT4IT02_1698W/tests host-test MODULE=mp3`
- 冒烟门：`make -C examples/L_CT4IT02_1698W/tests host-test-smoke MODULE=mp3`
  （用例 `test_duration_estimate_single_frame_cbr`，跑全量后 grep 该用例 PASS + OK）
- 单模块：`make -C examples/L_CT4IT02_1698W/demo/src/demo_mp3/test test`
- 单用例二进制：`demo/src/demo_mp3/test/out/demo_mp3_core_test`

说明：覆盖 MP3 帧头解析、采样率/码率表、时长估算、播放进度状态机与时间格式化，
全部为无 liot 依赖的纯算术逻辑。本版按核心测试优化计划将 9 个用例收敛为 6 个
（ID3/坏数据帧偏移合并、采样率/码率表全枚举收敛为表边界、删除空流时长用例，
防回归点不变）。

> 注：Unity 当前 `-n` 参数可解析但不跳过执行，下列「单独运行」命令会跑完整个二进制
> 并打印所有用例结果；若要只看目标用例，请配合 `grep '<用例名>.*PASS'` 使用。

## 1. test_frame_offset_skips_id3_junk_no_sync_returns_zero

- 被测函数：`mp3_frame_offset`
- 功能：在 ID3v2 标签与填充数据之后定位真正的 MPEG 帧同步头；流内未找到有效
  同步头时返回 0。
- 输入与期望：12 字节 ID3v2 头部（含 size 0）+ 0xFF 0xFB 同步头 → 偏移 12；
  `"xyz" + 0xFF 0x41`（0x41 不满足帧头条件）→ 0。
- 单独运行：`demo/src/demo_mp3/test/out/demo_mp3_core_test -n test_frame_offset_skips_id3_junk_no_sync_returns_zero`

## 2. test_sample_rate_and_bitrate_table_bounds

- 被测函数：`mp3_sample_rate`
- 功能：采样率表与码率表按版本/层/索引查表，非法索引、保留版本/保留位、非
  Layer III 均返回 0（合法代表值 + 越界代表值，不再逐项全枚举）。
- 输入与期望：采样率版本 3（MPEG1）索引 0/1/2 → 44100/48000/32000；版本 2 索引
  0/1/2 → 22050/24000/16000；索引 3、版本 0/4 越界 → 0。码率 MPEG1 L3 索引
  9/1/14 → 128/32/320；版本 2 L3 索引 8/13 → 64/144；索引 0/15/16、layer 2 → 0。
- 单独运行：`demo/src/demo_mp3/test/out/demo_mp3_core_test -n test_sample_rate_and_bitrate_table_bounds`

## 3. test_duration_estimate_single_frame_cbr

- 被测函数：`mp3_estimate_duration_ms`
- 功能：CBR 单帧时长估算（帧长 417B = 144×128000/44100，1152 采样 / 44.1kHz ≈ 26ms）。
- 输入：417 字节帧，帧头 0xFF 0xFB，128kbps、44100Hz。
- 期望：26ms。
- 单独运行：`demo/src/demo_mp3/test/out/demo_mp3_core_test -n test_duration_estimate_single_frame_cbr`

## 4. test_duration_estimate_walks_frames_skips_bad

- 被测函数：`mp3_estimate_duration_ms`
- 功能：跨多帧累加时长，帧间坏数据可跳过并继续寻找下一帧。
- 输入：帧 1（0..416）+ 30 字节 0xAA 坏区 + 帧 2（447..863）。
- 期望：52ms（两帧各 26ms）。
- 单独运行：`demo/src/demo_mp3/test/out/demo_mp3_core_test -n test_duration_estimate_walks_frames_skips_bad`

## 5. test_current_ms_state_machine

- 被测函数：`mp3_core_current_ms`
- 功能：按播放状态计算当前进度毫秒：LOADING/ERROR 恒为 0；STOPPED 仅在 finished
  时返回总时长；PAUSED 冻结在暂停起始点；PLAYING 扣除累计暂停并钳制到时长。
- 输入与期望（duration=1000）：
  - LOADING/ERROR → 0；STOPPED 未 finished → 0，finished → 1000
  - PAUSED → 123；PLAYING（now=600、started=100、paused=0）→ 500
  - PLAYING 含暂停累计 100 → 400；now 超长 → 钳制 1000
- 单独运行：`demo/src/demo_mp3/test/out/demo_mp3_core_test -n test_current_ms_state_machine`

## 6. test_format_time_and_state_name

- 被测函数：`mp3_format_time`、`mp3_state_name`
- 功能：毫秒格式化为 `MM:SS`，播放状态映射为字符串。
- 输入与期望：0 → `00:00`；61000 → `01:01`；3599999 → `59:59`；
  STOPPED/PLAYING/PAUSED/LOADING/ERROR 分别返回对应名；未知状态返回 `?`。
- 单独运行：`demo/src/demo_mp3/test/out/demo_mp3_core_test -n test_format_time_and_state_name`
