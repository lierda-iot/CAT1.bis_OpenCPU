#!/usr/bin/env python3
"""Exercise actual video buffer allocation and DMA-safe teardown functions."""
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
    parser.add_argument('--source', type=Path, help='Frozen tirtc_video.c for negative controls')
    parser.add_argument('--scenario', action='append', help='Run selected scenario names only')
    args = parser.parse_args()
    tests = Path(__file__).resolve().parent
    app = tests.parent
    source = (args.source or app / 'video/tirtc_video.c').read_text(encoding='utf-8')
    macros = source[source.index('#define CAP_W'):source.index('LIOT_ADD_CAMERA(')]
    declarations = source[source.index('static liot_task_t s_task;'):
                          source.index('/* Per-call counters')]
    names = ['set_error']
    # Frozen pre-fix code has no buffers_close helper. Its original
    # hardware_close is still extracted, so the failure control runs unchanged.
    if 'static void buffers_close(' in source:
        names.append('buffers_close')
    names += ['buffers_open', 'hardware_close']
    template = (tests / 'video_buffers_test.c').read_text(encoding='utf-8')
    unit_text = template.replace('/* PRODUCTION_MACROS */', macros).replace(
        '/* PRODUCTION_DECLARATIONS */', declarations).replace(
        '/* PRODUCTION_FUNCTIONS */', '\n'.join(function(source, name) for name in names))
    cases = []
    for owner in (2, 4):
        cases += [(name, owner, 'contiguous' if name == 'contiguous' else 'split') for name in (
            'contiguous', 'fragmented', 'malloc_fallback', 'reserve_before',
            'reserve_after', 'raw_too_fragmented', 'repeated')]
        cases += [(f'split_failure_{n}', owner, 'split')
                  for n in range(1, 4 if owner == 4 else 8)]
        for mode in ('contiguous', 'split'):
            cases += [(name, owner, mode) for name in (
                'hardware_pipeline', 'hardware_quiesce', 'hardware_deinit',
                'hardware_close_order')]
    if args.scenario:
        missing = set(args.scenario) - {case[0] for case in cases}
        if missing:
            parser.error('Unknown scenario(s): ' + ', '.join(sorted(missing)))
        cases = [case for case in cases if case[0] in args.scenario]
    cc = [args.cc] + (['cc'] if Path(args.cc).stem.lower() == 'zig' else [])
    with tempfile.TemporaryDirectory(prefix='pro-video-buffers-') as temporary:
        directory = Path(temporary)
        unit, binary = directory/'test.c', directory/'test.exe'
        unit.write_text(unit_text, encoding='utf-8')
        subprocess.run(cc + ['-std=c99', '-O0', '-I'+str(app/'video'),
                             str(unit), '-o', str(binary)], check=True)
        for name, owner, mode in cases:
            subprocess.run([str(binary), name, str(owner), mode], check=True)
    print(f'PASS: {len(cases)} production video allocation and DMA teardown scenarios')


if __name__ == '__main__':
    main()
