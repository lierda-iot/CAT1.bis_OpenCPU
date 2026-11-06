# 字库输入、来源与离线重建

本目录只保存字体生成输入，不包含旧版头像、首页图片或页面逻辑。固件编译使用 `../../font/tirtc_font_14.c`，不读取本目录的 gzip 文件。

## 输入来源

| 文件 | 来源与作用 |
|---|---|
| `lv_font_cn_14.c.gz` | [XiaoTai 固定提交 f718663 的 main/ui 字库](https://github.com/tangeai/xiaotai-esp32/tree/f7186630ed63f1e26838b8394e3e83a7e032646c/waveshare-esp32p4-xiaotai/main/ui)，原始 14px、4bpp、未压缩字形表。 |
| `supplemental_font_14.c.gz` | 已有工程中 `U+76CA 益` 的补字。原字库遗漏此字；补充位图来自当时的 SimHei 14px 生成结果，现直接复用 91 字节字形数据。 |
| `MANIFEST.json` | 两份解压后 C 源码的 SHA-256、补字字体来源哈希及原生成参数。脚本读取时强制校验。 |

弯单引号 `U+2018/U+2019`、弯双引号 `U+201C/U+201D` 和顿号 `U+3001` 都已存在于官方原字库。字体子集生成器保留已有字形的全部像素和尺寸，不重新绘制，也不读取本机字体文件。

## 动态 AI 字幕覆盖

UI 14 修复“查天气□看新闻□讲笑话”的缺字：旧子集没有选入顿号 `U+3001`。只扫描源码中的静态文字，无法覆盖运行时到达的 AI 字幕。生成器现在额外维护一个有界的字幕字符集合，包含中文常用标点、书名号/引号/括号、省略号/长短横线、全角 ASCII、温度/货币和常用数学符号。

原输入缺少的不换行空格/窄不换行空格、不换行连字符、数字横线、连接点、减号、波浪线、半角人民币符号和两种全角 ASCII 符号，复用同义或近似字形的原始像素。具体十项别名记录在生成器 `CAPTION_GLYPH_ALIASES` 和 `generation_report.json`；已有源字形始终优先。所有旧子集字形的像素和尺寸保持不变，不加入 emoji 字库。

## 授权说明

上游软件的 MIT 文本保留在 `../../LICENSE`。压缩的原字库 C 文件保留了字体名称及原生成参数，其中包括 SimHei、Noto Sans SC、Segoe 等字体来源；补字来源与哈希保留在清单中。现有输入没有另外附带所有原始字体的授权文本，因此这里不将软件 MIT 文本解释为对这些原始字体授予额外授权。

本工程没有打包 Windows 的 TTF/OTF 字体文件。生成器只重排并复制已有字形数据，不安装字体，不改变原字体的授权范围。

## 使用

在项目根目录执行（Python 3.10 及以上，仅标准库）：

```text
python examples/NT26FxDx_OpenKit/tirtc_pro/ui/tools/prepare_font.py --strict
```

上面的生成和表结构检查仅需 Python。可选的 `--verify-lvgl` 会在电脑上编译并运行真实 LVGL 字形检查，需要本机 C 编译器（`tcc`、`gcc` 或 `clang` 在 `PATH` 中，也可用 `TIRTC_HOST_CC` 指定可执行文件）；不能使用生成 ARM 固件的交叉编译器代替。准备好后在同一命令末尾加上 `--verify-lvgl`。这一步用于维护字库，不是正常固件编译的前提。


脚本保留全部 GB2312 的 6,763 个汉字，扫描当前应用的 `.c/.h/.inc` 字符串，并展开实际使用的 `LV_SYMBOL_*` 图标。只排除 `ui/tools` 内的生成工具。主字库缺失而 Montserrat 14 提供的图标会明确记录为 fallback；任何其余缺字都会失败。生成后的每个码点索引和位图范围都会再检查，真实 LVGL 验证还会拒绝占位框与空白的可打印字形。

未来新增接口文字可写入 `../../font/extra_characters.txt`，也可添加 `--extra-text <UTF-8文本文件>`。覆盖全部 GB2312 不等于覆盖任意 Unicode；新文字需要重新运行严格检查。

输出为 `../../font/tirtc_font_14.c/.h`、`included_characters.txt` 和 `generation_report.json`。生成器保留 `TIRTC_EXTERNAL_UI_ASSETS` 和 `tirtc_font_set_bitmap()` 接口，按新位图长度生成校验值；修改字库后必须同步重建外置资源安装包。源文件使用临时文件替换，避免编译器读到写入一半的字体 C 文件。
