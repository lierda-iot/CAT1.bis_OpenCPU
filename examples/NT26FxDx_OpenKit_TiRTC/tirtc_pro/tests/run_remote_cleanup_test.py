#!/usr/bin/env python3
"""Exercise production LIVE cleanup with late media/SDK release boundaries."""
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
    parser.add_argument('--source', type=Path, help='Frozen remote C for a negative control')
    args = parser.parse_args()
    tests = Path(__file__).resolve().parent
    app = tests.parent
    source = (args.source or app / 'remote/tirtc_remote.c').read_text(encoding='utf-8')
    declarations = source[source.index('#define REMOTE_ERROR'):source.index('static uint32_t now')]
    names = ['now', 'tirtc_remote_has_session', 'begin_close', 'cleanup_step', 'step']
    functions = '\n'.join(function(source, name) for name in names)
    ui_source = (app / 'ui/tirtc_ui.c').read_text(encoding='utf-8')
    ui_busy = function(ui_source, 'remote_session_active')
    template = (tests / 'remote_cleanup_test.c').read_text(encoding='utf-8')
    unit = template.replace('/* PRODUCTION_DECLARATIONS */', declarations).replace(
        '/* PRODUCTION_FUNCTIONS */', functions).replace('/* PRODUCTION_UI_BUSY_FUNCTION */', ui_busy)
    with tempfile.TemporaryDirectory(prefix='pro-remote-cleanup-test-') as output:
        code, binary = Path(output) / 'test.c', Path(output) / 'test.exe'
        code.write_text(unit, encoding='utf-8')
        cc = [args.cc]
        if Path(args.cc).stem.lower() == 'zig':
            cc.append('cc')
        subprocess.run(cc + ['-std=c99', '-O0', '-I' + str(app), str(code), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)


if __name__ == '__main__':
    main()
