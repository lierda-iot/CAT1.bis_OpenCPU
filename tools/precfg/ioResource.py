#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
根据 ini 生成 C 引脚表
规则：
  xxxN_default=1   -> 不生成任何引脚变量，该设备槽位为空
  xxxN_default=0   -> 使用自定义引脚（若存在）
"""
import os
import re
import argparse
from configparser import ConfigParser

PIN_ORDER = {                       # 各设备所需引脚顺序
    'uart': ['tx', 'rx', 'cts', 'rts'],
    'i2c':  ['scl', 'sda'],
    'spi':  ['sclk', 'ssn', 'mosi', 'miso'],
    'cspi': ['mclk', 'pclk', 'cs', 'sdo0', 'sdo1'],
    'i2s':  ['mclk', 'bclk', 'lrck', 'din', 'dout'],
    'can':  ['tx', 'rx', 'stb'],
}

def parse_ini(ini_path):
    cfg = ConfigParser(inline_comment_prefixes=(';',))
    # 关键：先读进来，再统一 strip 键与值
    with open(ini_path, encoding='utf-8') as f:
        raw_lines = [l.lstrip() for l in f]      # 去掉每行前面所有空格/Tab
    cfg.read_string(''.join(raw_lines))

    data = {k: {} for k in PIN_ORDER}
    for sect in cfg.sections():
        prefix = sect.lower()
        if prefix not in PIN_ORDER:
            continue
        order = PIN_ORDER[prefix]
        for opt in cfg.options(sect):
            # 键本身已经 strip 过，值再 strip 一次
            val = cfg.get(sect, opt).strip()
            # _default
            m = re.match(rf'{prefix}(\d+)_default', opt)
            if m:
                idx = int(m.group(1))
                use_def = val == '1'
                data[prefix].setdefault(idx, {})
                data[prefix][idx]['_use_default'] = use_def
                continue
            # _pin_xxx
            m = re.match(rf'{prefix}(\d+)_pin_(\w+)', opt)
            if m:
                idx, pin = int(m.group(1)), m.group(2)
                if pin not in order:
                    continue
                data[prefix].setdefault(idx, {})
                paddr, func = map(int, val.split(','))
                data[prefix][idx][pin] = (paddr, func)
    return data

def generate_c(data, c_path):
    lines = ['#include "liot_iocfg.h"', '#define __L_IO_MAP_ __attribute__((section(".iomap")))', ]

    # 1. 仅生成“ini 里真正出现”的 ioPin 变量
    for prefix in PIN_ORDER:
        for idx in sorted(data[prefix]):
            if data[prefix][idx].get('_use_default'):
                continue
            for pin in PIN_ORDER[prefix]:
                if pin in data[prefix][idx]:
                    p, f = data[prefix][idx][pin]
                    lines.append(f'__L_IO_MAP_ ioPin {prefix}{idx}_pin_{pin} = {{{p}, {f}}};')
        lines.append('')

    # 2. 按 0..max 连续填充数组
    for prefix, pins in PIN_ORDER.items():
        struct_name = prefix.capitalize() + 'driveio'
        lines.append(f'__L_IO_MAP_ {struct_name} {prefix}IoResource[] = {{')

        max_idx = max(data[prefix], default=-1)          # 找最大索引
        for idx in range(max_idx + 1):
            if idx not in data[prefix] or data[prefix][idx].get('_use_default'):
                lines.append('    {},')                # 空占位
            else:
                ptrs = []
                for p in pins:
                    ptrs.append(f'&{prefix}{idx}_pin_{p}' if p in data[prefix][idx] else '(ioPin*)0')
                lines.append(f'    {{{", ".join(ptrs)}}},')
        lines.append('    {(ioPin*)0xAAAAAA,},')
        lines.append('};')
        lines.append('')

    with open(c_path, 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines))
    # print(f'已生成 {c_path}')

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('-i', '--input', required=True, help='输入 ini')
    ap.add_argument('-o', '--output', required=True, help='输出 c')
    args = ap.parse_args()

    if not os.path.isfile(args.input):
        print(f'输入配置文件不存在: {args.input}')
        exit(1)

    generate_c(parse_ini(args.input), args.output)

if __name__ == '__main__':
    main()