#!/usr/bin/env python3
"""Exercise production call cleanup/control with delayed HTTP and busy media gates."""
import argparse
from pathlib import Path
import subprocess
import tempfile
import sys
sys.dont_write_bytecode = True
from run_calls_audio_lifecycle_test import function


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cc', default='cc')
    parser.add_argument('--source', type=Path, help='Frozen calls C for a negative control')
    args = parser.parse_args()
    tests = Path(__file__).resolve().parent
    app = tests.parent
    source = (args.source or app / 'calls/tirtc_calls.c').read_text(encoding='utf-8')
    declarations = source[source.index('#define CALL_ERROR'):source.index('static uint32_t tick')]
    names = ['tick', 'due', 'copy', 'context_equal', 'set_phase',
             'tirtc_calls_has_session', 'callback_current', 'job_free',
             'queue_job_action', 'queue_job', 'clear_job', 'process_job',
             'cleanup_step', 'step', 'tirtc_calls_dial_contact']
    functions = '\n'.join(function(source, name) for name in names)
    template = (tests / 'calls_cleanup_test.c').read_text(encoding='utf-8')
    unit = template.replace('/* PRODUCTION_DECLARATIONS */', declarations).replace(
        '/* PRODUCTION_FUNCTIONS */', functions)
    with tempfile.TemporaryDirectory(prefix='pro-call-cleanup-test-') as output:
        code, binary = Path(output) / 'test.c', Path(output) / 'test.exe'
        code.write_text(unit, encoding='utf-8')
        cc = [args.cc]
        if Path(args.cc).stem.lower() == 'zig':
            cc.append('cc')
        subprocess.run(cc + ['-std=c99', '-O0', '-I' + str(app), str(code), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)


if __name__ == '__main__':
    main()
