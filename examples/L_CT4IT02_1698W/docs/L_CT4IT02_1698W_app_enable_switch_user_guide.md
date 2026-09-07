# L_CT4IT02_1698W 应用开关使用说明

本文说明 `BUILD_MODE=app` 下如何按需选择应用并编译固件。

## 1. 应用开关

在构建命令中使用 `y` 启用、`n` 关闭：

| 开关 | 应用 |
| --- | --- |
| `APP_WATCHFACE_EN` | 表盘 |
| `APP_BAJI_EN` | 吧唧 |
| `APP_ATTITUDE_EN` | 体感 |
| `APP_ATTFUN_EN` | 体感模式 |
| `APP_SALARY_EN` | 计算器 |
| `APP_MAP_EN` | 地图/定位 |
| `APP_MP3_EN` | MP3 音乐 |

至少启用一个应用。

## 2. 编译示例

默认配置为仅启用吧唧：

```bash
make all PROJECT=L_CT4IT02_1698W \
  MODEM=NT26F9D0 MODEMPKG=F9D_A \
  BUILD_MODE=app
```

仅启用表盘：

```bash
make all PROJECT=L_CT4IT02_1698W \
  MODEM=NT26F9D0 MODEMPKG=F9D_A \
  BUILD_MODE=app \
  APP_WATCHFACE_EN=y APP_BAJI_EN=n APP_ATTITUDE_EN=n \
  APP_ATTFUN_EN=n APP_SALARY_EN=n APP_MAP_EN=n APP_MP3_EN=n
```

仅启用地图：

```bash
make all PROJECT=L_CT4IT02_1698W \
  MODEM=NT26F9D0 MODEMPKG=F9D_A \
  BUILD_MODE=app \
  APP_WATCHFACE_EN=n APP_BAJI_EN=n APP_ATTITUDE_EN=n \
  APP_ATTFUN_EN=n APP_SALARY_EN=n APP_MAP_EN=y APP_MP3_EN=n
```

## 3. 当前行为

- 只启用一个应用时，上电直接进入该应用。
- 被关闭的应用不会进入对应功能页。
- `APP_MAP_EN=y` 会同时启用地图/GNSS 相关依赖。
- `APP_SALARY_EN=y` 和 `APP_MP3_EN=y` 会启用对应模块依赖。
- 多应用菜单仍在分阶段验证中。当前测试中，部分其它应用的菜单界面可能仍显示，但不能进入；暂不将多应用组合视为最终交付行为。

## 4. 注意事项

- 这些开关仅用于 `BUILD_MODE=app`。
- 每次切换开关后建议执行完整编译。
- 单应用 detail 页的返回行为仍需结合实机继续确认与测试。
