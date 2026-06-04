#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import argparse
import re
import os
from collections import OrderedDict

def normalize_macro_name(s: str) -> str:
    return re.sub(r'[^0-9A-Za-z_]', '_', s.upper())

def parse_ini(path):
    """
    返回 OrderedDict:
        { 'SECTION': [(key, value), ...], ... }
    保持 ini 中的段落顺序
    """
    sections = OrderedDict()
    curr = 'GLOBAL'
    with open(path, encoding='utf-8') as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith(';') or line.startswith('#'):
                continue
            if line.startswith('[') and line.endswith(']'):
                curr = line[1:-1].strip()
                continue
            if '=' not in line:
                continue
            k, v = line.split('=', 1)
            k, v = k.strip(), v.strip()
            # 去掉行尾注释
            v = re.split(r'[;#]', v)[0].strip()
            # 字符串去引号
            if len(v) >= 2 and ((v[0] == '"' and v[-1] == '"') or (v[0] == "'" and v[-1] == "'")):
                v = v[1:-1]
            sections.setdefault(curr, []).append((k, v))
    return sections

def build_header(sect_dict, header_guard='__INI_CONFIG_H__'):
    lines = [
        f'#ifndef {header_guard}',
        f'#define {header_guard}',
        ''
    ]
    for sect, kvs in sect_dict.items():
        lines.append(f'/*--------------[{sect}]--------------------*/')
        for k, v in kvs:
            macro = '__INICFG_' + normalize_macro_name(k) + '__'
            # 数字直接写，其余加引号
            try:
                int(v, 0)
                val = v
            except ValueError:
                val = f'"{v}"'
            lines.append(f'#define {macro} {val}')
        lines.append('')
    lines.append(f'#endif /* {header_guard} */')
    return '\n'.join(lines)

def main():
    parser = argparse.ArgumentParser(description='将 ini 文件转换为 C 头文件宏定义')
    parser.add_argument('-i', '--input', required=True, help='输入 ini 文件路径')
    parser.add_argument('-o', '--output', required=True, help='输出头文件路径')
    args = parser.parse_args()

    if not os.path.isfile(args.input):
        print(f'错误: 找不到输入文件 {args.input}')
        exit(2)

    sect_dict = parse_ini(args.input)
    header = build_header(sect_dict)
    with open(args.output, 'w', encoding='utf-8') as f:
        f.write(header)

if __name__ == '__main__':
    main()