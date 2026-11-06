#!/usr/bin/env python3
"""Check Room playback PCM continuity and scheduling with deterministic fakes."""
import argparse
import hashlib
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from run_group_backend_test import CJSON_SHA256, compiler_command

CASES = ['packet_boundaries', 'batch_refill', 'unfinished_playback', 'format_filter',
         'initial_prebuffer', 'odd_tail_wait', 'odd_tail_joins_next_packet',
         'jitter_continuity', 'new_burst', 'fractional_time', 'busy_without_replay',
         'play_ahead_bound', 'teardown_pending_audio', 'clock_wrap',
         'expired_partial_refresh', 'diagnostic_windows']


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cc', default='tcc', help='C compiler executable (TinyCC or Zig)')
    parser.add_argument('--cjson', required=True, type=Path,
                        help='Official cJSON v1.7.16 cJSON.c')
    parser.add_argument('--aec', choices=('0', '1'), default='0')
    args = parser.parse_args()
    cases = CASES + (['aec_backpressure', 'aec_zero_warmup', 'aec_real_error']
                     if args.aec == '1' else [])
    try:
        cc = compiler_command(args.cc)
    except ValueError as error:
        parser.error(str(error))
    if hashlib.sha256(args.cjson.read_bytes()).hexdigest() != CJSON_SHA256:
        parser.error('Expected unmodified upstream cJSON v1.7.16 cJSON.c')
    app = Path(__file__).resolve().parents[1]
    root = app.parents[2]
    with tempfile.TemporaryDirectory(prefix='pro-room-audio-') as output:
        exe = Path(output) / 'test.exe'
        includes = [app/'tests/host_stubs', app/'ui', app/'media', app/'platform',
                    app/'runtime', app/'sdk/include', root/'components/thirdparty/CJSON']
        flags = ['-DHWDEMO_GROUP_ROOM_EN', '-DCJSON_HIDE_SYMBOLS', '-UWIN32',
                 '-DDEMO_AI_AUDIO_AEC_ENABLE=' + args.aec]
        if len(cc) == 1:
            flags += ['-D__GNUC__=4']
        flags += ['-I' + p.as_posix() for p in includes]
        flags += ['-o', exe.as_posix(), (app/'tests/group_audio_test.c').as_posix(),
                  (app/'media/g711_codec.c').as_posix(), args.cjson.resolve().as_posix()]
        rsp = Path(output) / 'args.rsp'
        rsp.write_text('\n'.join('"' + f + '"' for f in flags), encoding='utf-8')
        subprocess.run(cc + ['@' + str(rsp)], check=True)
        failed = [case for case in cases
                  if subprocess.run([str(exe), case], check=False).returncode]
        if failed:
            raise SystemExit('FAILED Room audio scenarios: ' + ', '.join(failed))
    print(f'PASS: {len(cases)} Room audio scenarios (AEC={args.aec})')


if __name__ == '__main__':
    main()
