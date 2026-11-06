#!/usr/bin/env python3
"""Run actual pro UI/LVGL with a simulated pointer (no hardware or network)."""
import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cc', default='tcc', help='TinyCC executable')
    args = parser.parse_args()
    cc = shutil.which(args.cc)
    if not cc:
        parser.error('TinyCC not found; pass --cc with its executable path')
    app = Path(__file__).resolve().parents[1]
    root = app.parents[2]
    ui = app / 'ui'
    lv = root / 'components/thirdparty/lvgl/lvgl-8.3.10'
    with tempfile.TemporaryDirectory(prefix='pro-room-ui-') as output:
        exe = Path(output) / 'test.exe'
        flags = ['-DLV_CONF_PATH=' + (ui/'lv_conf.h').as_posix(),
                 '-I' + ui.as_posix(), '-I' + lv.as_posix(), '-o', exe.as_posix()]
        flags += [p.as_posix() for p in (lv/'src').rglob('*.c')]
        flags += [p.as_posix() for p in ui.rglob('*.c')]
        flags += [(app/'tests/ui_room_test.c').as_posix()]
        rsp = Path(output) / 'args.rsp'
        rsp.write_text('\n'.join('"' + f + '"' for f in flags), encoding='utf-8')
        subprocess.run([cc, '@' + str(rsp)], check=True)
        subprocess.run([str(exe)], check=True)

if __name__ == '__main__':
    main()
