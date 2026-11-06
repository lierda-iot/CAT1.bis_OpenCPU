#!/usr/bin/env python3
"""Run Room signaling regressions against the production owner, without I/O."""
import argparse
import hashlib
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from run_group_backend_test import CJSON_SHA256, compiler_command

CASES = ['scoped_pin', 'raw_pin', 'mic_states', 'self_ptt', 'ack_and_business_id']


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cc', default='tcc', help='C compiler executable (TinyCC or Zig)')
    parser.add_argument('--cjson', required=True, type=Path,
                        help='Official cJSON v1.7.16 cJSON.c')
    args = parser.parse_args()
    try:
        cc = compiler_command(args.cc)
    except ValueError as error:
        parser.error(str(error))
    if hashlib.sha256(args.cjson.read_bytes()).hexdigest() != CJSON_SHA256:
        parser.error('Expected unmodified upstream cJSON v1.7.16 cJSON.c')
    app = Path(__file__).resolve().parents[1]
    root = app.parents[2]
    with tempfile.TemporaryDirectory(prefix='pro-room-protocol-') as output:
        exe = Path(output) / 'test.exe'
        includes = [app/'tests/host_stubs', app/'ui', app/'media', app/'platform',
                    app/'runtime', app/'sdk/include', root/'components/thirdparty/CJSON']
        flags = ['-DHWDEMO_GROUP_ROOM_EN', '-DCJSON_HIDE_SYMBOLS', '-UWIN32']
        if len(cc) == 1:
            flags += ['-D__GNUC__=4']
        flags += ['-I' + p.as_posix() for p in includes]
        flags += ['-o', exe.as_posix(), (app/'tests/group_protocol_test.c').as_posix(),
                  (app/'media/g711_codec.c').as_posix(), args.cjson.resolve().as_posix()]
        subprocess.run(cc + flags, check=True)
        failed = [case for case in CASES
                  if subprocess.run([str(exe), case], check=False).returncode]
        if failed:
            raise SystemExit('FAILED Room protocol scenarios: ' + ', '.join(failed))
    print(f'PASS: {len(CASES)} Room protocol scenarios')


if __name__ == '__main__':
    main()
