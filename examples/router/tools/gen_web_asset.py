#!/usr/bin/env python3
"""
gen_web_asset.py — 将 web/ui/ 下的前端文件 gzip 压缩后生成 C 数组。

用法:
    python3 gen_web_asset.py <www_dir> <out_c> <out_h>

生成 liot_router_resource.c/.h，内含每个文件的 gzip 数据与索引表 gWebAssets[]。
httpd 静态 handler 按请求路径查表返回，带 Content-Encoding: gzip。

维护方式：只改 web/ui/ 下的真实网页文件，构建时自动重新生成本产物，
不要手改生成的 .c。
"""
import sys, os, gzip

# 路径 → MIME
MIME = {
    '.html': 'text/html',
    '.js':   'application/javascript',
    '.css':  'text/css',
    '.ico':  'image/x-icon',
    '.png':  'image/png',
    '.svg':  'image/svg+xml',
    '.json': 'application/json',
}

def c_name(fn):
    return 'asset_' + ''.join(ch if ch.isalnum() else '_' for ch in fn)

def minify_text(text, ext):
    """保守精简：仅去注释，不动缩进/行内内容(保护模板字符串、字符串字面量)。
    - .js : 去整行 // 注释 + /* */ 块注释(本项目无字符串内 // 或 /*)
    - .css/.html 内联 <style>: 去 /* */
    - .html: 去 <!-- --> 注释
    源文件不变，只精简打进固件的副本。"""
    import re
    if ext == '.js':
        text = re.sub(r'/\*.*?\*/', '', text, flags=re.S)      # 块注释
        text = re.sub(r'(?m)^\s*//.*$', '', text)              # 整行行注释
    if ext in ('.html', '.css'):
        text = re.sub(r'<!--.*?-->', '', text, flags=re.S)     # HTML 注释
        text = re.sub(r'/\*.*?\*/', '', text, flags=re.S)      # CSS 注释
    text = re.sub(r'\n\s*\n+', '\n', text)                     # 合并空行
    return text

def main():
    if len(sys.argv) != 4:
        print(__doc__); sys.exit(1)
    www, out_c, out_h = sys.argv[1], sys.argv[2], sys.argv[3]

    MINIFY_EXT = ('.js', '.html', '.css')
    PRECOMPRESSED_EXT = ('.png', '.jpg', '.jpeg', '.gz', '.ico', '.webp')
    files = []
    for root, _, names in os.walk(www):
        for nm in sorted(names):
            full = os.path.join(root, nm)
            rel = os.path.relpath(full, www).replace('\\', '/')
            ext = os.path.splitext(nm)[1].lower()
            mime = MIME.get(ext, 'application/octet-stream')
            with open(full, 'rb') as f:
                raw = f.read()
            if ext in MINIFY_EXT:
                try:
                    raw = minify_text(raw.decode('utf-8'), ext).encode('utf-8')
                except UnicodeDecodeError:
                    pass  # 非文本，原样打包
            # 已压缩格式(png/jpg/gz/ico)再 gzip 是白费(甚至变大)，直接存原始字节
            if ext in PRECOMPRESSED_EXT:
                data, gzipped = raw, 0
            else:
                data, gzipped = gzip.compress(raw, 9), 1
            files.append((rel, mime, data, gzipped))

    with open(out_c, 'w', encoding='utf-8') as c:
        c.write('/* 自动生成，勿手改。源: web/ui/  生成脚本: tools/gen_web_asset.py */\n')
        c.write('#include "liot_router_resource.h"\n\n')
        for rel, mime, data, gzipped in files:
            var = c_name(rel)
            tag = 'gzip' if gzipped else 'raw'
            c.write(f'/* {rel} ({len(data)} bytes {tag}) */\n')
            c.write(f'static const unsigned char {var}[] = {{')
            c.write(','.join(str(b) for b in data))
            c.write('};\n\n')
        c.write('const Liot_WebAsset_t gWebAssets[] = {\n')
        for rel, mime, data, gzipped in files:
            var = c_name(rel)
            path = '/' + rel
            c.write(f'    {{"{path}", "{mime}", {var}, sizeof({var}), {gzipped}}},\n')
            if rel == 'index.html':
                c.write(f'    {{"/", "{mime}", {var}, sizeof({var}), {gzipped}}},\n')
        c.write('};\n')
        c.write('const unsigned int gWebAssetCount = sizeof(gWebAssets)/sizeof(gWebAssets[0]);\n')

    with open(out_h, 'w', encoding='utf-8') as h:
        h.write('/* 自动生成，勿手改。 */\n')
        h.write('#ifndef __WWW_ASSET_H__\n#define __WWW_ASSET_H__\n\n')
        h.write('typedef struct {\n'
                '    const char *path;\n'
                '    const char *mime;\n'
                '    const unsigned char *data;\n'
                '    unsigned int len;\n'
                '    unsigned char gzipped;   /* 1=data 为 gzip，需带 Content-Encoding */\n'
                '} Liot_WebAsset_t;\n\n')
        h.write('extern const Liot_WebAsset_t gWebAssets[];\n')
        h.write('extern const unsigned int gWebAssetCount;\n\n')
        h.write('#endif\n')

    total = sum(len(d) for _, _, d, _ in files)
    print(f'[gen_web_asset] {len(files)} files, {total} bytes → {out_c}')

if __name__ == '__main__':
    main()
