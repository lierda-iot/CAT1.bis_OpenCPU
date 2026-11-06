#!/usr/bin/env python3
"""Run the actual Pro settings screen with LVGL and a simulated pointer."""
import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cc', default='cc', help='C compiler executable (also accepts Zig)')
    args = parser.parse_args()
    cc = shutil.which(args.cc)
    if not cc:
        parser.error('Compiler not found; pass --cc with its executable path')
    command = [cc] + (['cc'] if Path(cc).stem.lower() == 'zig' else [])
    app = Path(__file__).resolve().parents[1]
    ui = app / 'ui'
    lv = app.parents[2] / 'components/thirdparty/lvgl/lvgl-8.3.10'
    with tempfile.TemporaryDirectory(prefix='pro-settings-mic-') as output:
        exe = Path(output) / 'test.exe'
        flags = ['-std=c99', '-O1', '-UNDEBUG',
                 '-DLV_CONF_PATH=' + (ui / 'lv_conf.h').as_posix(),
                 '-I' + ui.as_posix(), '-I' + lv.as_posix(), '-o', exe.as_posix()]
        flags += [p.as_posix() for p in sorted((lv / 'src').rglob('*.c'))]
        flags += [p.as_posix() for p in sorted(ui.rglob('*.c'))]
        flags += [(app / 'tests/settings_mic_test.c').as_posix()]
        rsp = Path(output) / 'args.rsp'
        rsp.write_text('\n'.join('"' + f + '"' for f in flags), encoding='utf-8')
        subprocess.run(command + ['@' + str(rsp)], check=True)
        subprocess.run([str(exe)], check=True)


if __name__ == '__main__':
    main()
