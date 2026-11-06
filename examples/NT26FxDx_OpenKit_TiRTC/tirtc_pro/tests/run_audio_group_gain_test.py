#!/usr/bin/env python3
"""Check production audio owner gain and handoff using SDK/RTOS fakes."""
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
    with tempfile.TemporaryDirectory(prefix='pro-audio-group-gain-') as output:
        for room in (False, True):
            exe = Path(output) / ('gain-room.exe' if room else 'gain-original.exe')
            flags = ['-Wall', '-Werror']
            if room:
                flags.append('-DHWDEMO_GROUP_ROOM_EN')
            flags += ['-I' + (app / 'tests/audio_gain_host_stubs').as_posix(),
                      '-I' + (app.parents[2] / 'components/kernel/lierda_api/liot_audio2').as_posix(),
                      '-o', exe.as_posix(),
                      (app / 'tests/audio_group_gain_test.c').as_posix(),
                      (app / 'media/audio_device.c').as_posix()]
            rsp = Path(output) / 'args.rsp'
            rsp.write_text('\n'.join('"' + flag + '"' for flag in flags), encoding='utf-8')
            subprocess.run([cc, '@' + str(rsp)], check=True)
            subprocess.run([str(exe)], check=True)
    print('PASS: gain checks with Room support enabled and disabled')


if __name__ == '__main__':
    main()
