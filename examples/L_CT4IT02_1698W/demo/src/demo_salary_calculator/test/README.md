# salary 用例功能与使用说明

- 模块目录：`demo/src/demo_salary_calculator/test`
- 被测 seam：`demo/src/demo_salary_calculator/salary_core_utils.{c,h}`（与生产实现同一份源码）
- 用例总数：5
- 运行全部：`make -C examples/L_CT4IT02_1698W/tests host-test MODULE=salary`
- 单模块：`make -C examples/L_CT4IT02_1698W/demo/src/demo_salary_calculator/test test`
- 单用例二进制：`demo/src/demo_salary_calculator/test/out/salary_core_utils_test`

说明：覆盖日历/时区加减、NVM 配置校验与默认值、文本数字解析、按工作时段折算收入，
均为纯逻辑，生产文件中的 liot RTC/NVM 适配层不在本用例范围。

> 注：Unity 当前 `-n` 参数可解析但不跳过执行，下列「单独运行」命令会跑完整个
> 二进制并打印所有用例结果；若要只看目标用例，请配合 `grep '<用例名>.*PASS'` 使用。

## 1. test_leap_year_and_days_in_month

- 被测函数：`salary_is_leap_year`、`salary_days_in_month`
- 功能：闰年判定与每月天数（含 2 月、非法月份回退 31）。
- 输入与期望：2000/2024 闰，1900/2025 非闰；2024-02→29、2025-02→28、
  1 月→31、4 月→30、月 0/13 →31。
- 单独运行：`demo/src/demo_salary_calculator/test/out/salary_core_utils_test -n test_leap_year_and_days_in_month`

## 2. test_add_hours_across_day_month_year

- 被测函数：`salary_time_add_hours`
- 功能：对 `salary_time_fields_t` 做 ±小时加减，正确处理跨日/跨月/跨年与闰日回退。
- 输入与期望：
  - 2026-02-28 23:30 +2h → 2026-03-01 01:30
  - 2024-02-29 22:00 +6h → 2024-03-01 04:00
  - 2026-12-31 23:00 +2h → 2027-01-01 01:00
  - 2026-03-01 00:00 -2h → 2026-02-28 22:00
  - 2026-01-01 00:00 -2h → 2025-12-31 22:00
- 单独运行：`demo/src/demo_salary_calculator/test/out/salary_core_utils_test -n test_add_hours_across_day_month_year`

## 3. test_config_checksum_valid_defaults_prepare_save

- 被测函数：`salary_config_init_defaults`、`salary_config_is_valid`、`salary_config_prepare_save`
- 功能：默认配置初始化、checksum 完整性校验、保存前修正非法字段。
- 输入与期望：默认月薪 10000、响铃间隔 1、工作时段 9:00-18:00，`is_valid=true`；
  全 0 配置非法；`sound_interval=0` 经 `prepare_save` 修正为 1 后合法；
  翻转 checksum 一位后非法。
- 单独运行：`demo/src/demo_salary_calculator/test/out/salary_core_utils_test -n test_config_checksum_valid_defaults_prepare_save`

## 4. test_parse_digits_filters_mixed_text

- 被测函数：`salary_parse_digits`
- 功能：从混合文本中连续抽取十进制数字，无有效数字时返回 fallback。
- 输入与期望：`a1b2c3` → 123；`22,000元` → 22000；`0` → fallback 99；
  空串/NULL/纯文本 → 99。
- 单独运行：`demo/src/demo_salary_calculator/test/out/salary_core_utils_test -n test_parse_digits_filters_mixed_text`

## 5. test_earned_cent_window_and_rate

- 被测函数：`salary_earned_cent`
- 功能：按工作时段窗口计算已赚取金额（分），时段外为 0，时段内按日薪线性折算。
- 输入与期望（默认配置 9:00-18:00、22 个工作日）：
  - 8:59:59、9:00:00 → 0 分（起点不开始计薪）
  - 18:00 → 45454 分（10000 元日薪 1000000 分 ÷ 22）
  - 19:00 → 仍为 45454 分（超过时段上限不额外增加）
  - 月薪 22000：18:00 → 100000 分；13:30 → 50000 分
  - `end_hour=9` 且 12:00 → 0 分（结束早于开始）
- 单独运行：`demo/src/demo_salary_calculator/test/out/salary_core_utils_test -n test_earned_cent_window_and_rate`
