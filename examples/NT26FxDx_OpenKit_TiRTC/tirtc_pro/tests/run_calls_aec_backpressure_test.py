#!/usr/bin/env python3
"""Exercise production call RX/capture functions with AEC on and off."""
import argparse
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from run_calls_audio_lifecycle_test import function


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cc', required=True)
    parser.add_argument('--source', type=Path, help='Optional frozen calls_audio.c for negative controls')
    parser.add_argument('--aec', choices=('0', '1', 'both'), default='both')
    parser.add_argument('--scenario', action='append', help='Run only the named scenario(s)')
    args = parser.parse_args()
    tests = Path(__file__).resolve().parent
    app = tests.parent
    source = (args.source or app / 'calls/calls_audio.c').read_text(encoding='utf-8')
    declarations = source[source.index('#define RX_BYTES'):source.index('static uint32_t now(void)')]
    names = ['now', 'before', 'audio_error', 'failure_log', 'fatal_error',
             'audio_session_locked', 'speaker_allowed_locked', 'microphone_allowed_locked',
             'effective_mic_level_locked', 'dev_diagnostic_record', 'discard_rx',
             'consume_rx_locked', 'capture_once', 'calls_audio_receive',
             'apply_controls', 'calls_audio_step']
    template = (tests / 'calls_aec_backpressure_test.c').read_text(encoding='utf-8')
    unit_text = template.replace('/* PRODUCTION_DECLARATIONS */', declarations).replace(
        '/* PRODUCTION_FUNCTIONS */', '\n'.join(function(source, name) for name in names))
    cc = [args.cc] + (['cc'] if Path(args.cc).stem.lower() == 'zig' else [])
    includes = [tests/'host_stubs', app/'calls', app/'media', app/'runtime',
                app/'sdk/include']
    backends = (0, 1) if args.aec == 'both' else (int(args.aec),)
    scenarios = args.scenario or ('busy', 'fatal', 'mute', 'record_busy',
        'timestamp_continuous', 'timestamp_timeout', 'timestamp_pause', 'timestamp_wrap')
    count = 0
    with tempfile.TemporaryDirectory(prefix='pro-calls-aec-') as temporary:
        directory = Path(temporary)
        unit = directory/'test.c'
        unit.write_text(unit_text, encoding='utf-8')
        for aec in backends:
            binary = directory/f'aec-{aec}.exe'
            subprocess.run(cc + ['-std=c99', '-O0', f'-DDEMO_AI_AUDIO_AEC_ENABLE={aec}',
                *['-I'+str(p) for p in includes], str(unit), str(app/'media/g711_codec.c'),
                '-o', str(binary)], check=True)
            for owner in (2, 3, 4):
                for scenario in scenarios:
                    subprocess.run([str(binary), str(owner), scenario], check=True)
                    count += 1
    print(f'PASS: {count} call/LIVE backpressure, capture, mute and timestamp scenarios')


if __name__ == '__main__':
    main()
