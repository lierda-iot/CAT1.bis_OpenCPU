#!/usr/bin/env python3
"""Compile the real pro Room worker and G.711 codec with deterministic I/O fakes."""
import argparse
import hashlib
from pathlib import Path
import shutil
import subprocess
import tempfile

# Same release as components/thirdparty/CJSON/cJSON.h; firmware ships an ARM library.
CJSON_SHA256 = 'fdfd427d82fadb395076567edf470c80cebee319e38fd417198508fe11ae56e7'
CASES = ['http', 'assignment', 'ack', 'ptt', 'capture', 'playback',
         'stale_http', 'preemption', 'network', 'commands', 'foreground', 'resume',
         'cleanup_order', 'audio_config', 'invalid_ack', 'token_lease',
         'init1', 'init2', 'init3', 'init4', 'init5',
         'queued_control_rebind', 'session_context_schedule', 'connect_context_submit',
         'late_control_navigation', 'fast_join_warm', 'join_warm_wrap',
         'join_warm_cancel', 'join_warm_levels', 'join_warm_timeout',
         'join_warm_muted_restore', 'join_warm_first_volume',
         'join_warm_early_ack', 'joined_levels_busy', 'joined_levels_failure',
         'capture_timeout_recover', 'capture_timeout_persistent', 'capture_fault',
         'capture_invalid_lease', 'capture_timeout_release', 'capture_timeout_suspend',
         'capture_timeout_ptt_expiry', 'capture_timeout_quick_ptt', 'capture_timeout_mute',
         'capture_timeout_segment', 'capture_timeout_success_reset', 'capture_timeout_wrap',
         'capture_timeout_close']


def compiler_command(executable):
    """Keep the host compiler selectable; Zig exposes its C driver as a subcommand."""
    cc = shutil.which(executable)
    if not cc:
        raise ValueError('C compiler not found; pass --cc with its executable path')
    return [cc, 'cc'] if Path(cc).stem.lower() == 'zig' else [cc]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cc', default='tcc', help='C compiler executable (TinyCC or Zig)')
    parser.add_argument('--cjson', required=True, type=Path,
                        help='Official cJSON v1.7.16 cJSON.c (kept outside firmware)')
    parser.add_argument('--aec', choices=(0, 1), type=int, default=0,
                        help='Compile the real Room worker with AEC enabled or disabled')
    args = parser.parse_args()
    try:
        cc = compiler_command(args.cc)
    except ValueError as error:
        parser.error(str(error))
    if hashlib.sha256(args.cjson.read_bytes()).hexdigest() != CJSON_SHA256:
        parser.error('Expected unmodified upstream cJSON v1.7.16 cJSON.c')
    app = Path(__file__).resolve().parents[1]
    root = app.parents[2]
    with tempfile.TemporaryDirectory(prefix='pro-room-backend-') as output:
        exe = Path(output) / 'test.exe'
        includes = [app/'tests/host_stubs', app/'ui', app/'media', app/'platform',
                    app/'runtime', app/'sdk/include', root/'components/thirdparty/CJSON']
        flags = ['-DHWDEMO_GROUP_ROOM_EN', '-DCJSON_HIDE_SYMBOLS', '-UWIN32',
                 '-DDEMO_AI_AUDIO_AEC_ENABLE=%d' % args.aec]
        if len(cc) == 1:
            flags += ['-D__GNUC__=4']
        flags += ['-I' + p.as_posix() for p in includes]
        flags += ['-o', exe.as_posix(), (app/'tests/group_backend_test.c').as_posix(),
                  (app/'media/g711_codec.c').as_posix(), args.cjson.resolve().as_posix()]
        rsp = Path(output) / 'args.rsp'
        rsp.write_text('\n'.join('"' + f + '"' for f in flags), encoding='utf-8')
        subprocess.run(cc + ['@' + str(rsp)], check=True)
        # Separate processes reset every production static field without a test-only reset API.
        failed = [case for case in CASES
                  if subprocess.run([str(exe), case], check=False).returncode]
        if failed:
            raise SystemExit('FAILED Room scenarios: ' + ', '.join(failed))
    print(f'PASS: {len(CASES)} Room backend scenarios; AEC={args.aec}')


if __name__ == '__main__':
    main()
