#!/usr/bin/env python3
"""Run Pro's actual AEC build-stamp declarations with the bundled SDK make/sh.

The consumer is a tiny fixture; stamp comparison and its dependency declaration
are extracted verbatim from tirtc_app.mk. No firmware compilation is performed.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import time


def declaration(source, name):
    matches = re.findall(r'^' + re.escape(name) + r'\s*:=.*$', source, re.MULTILINE)
    if len(matches) != 1 or matches[0].endswith('\\'):
        raise AssertionError('Expected one complete production declaration: ' + name)
    return matches[0]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, help='Alternative tirtc_app.mk for a negative control')
    parser.add_argument('--report', type=Path, help='Write results including a failed assertion')
    args = parser.parse_args()
    app = Path(__file__).resolve().parents[1]
    root = app.parents[2]
    source_path = (args.source or app / 'tirtc_app.mk').resolve()
    source = source_path.read_text(encoding='utf-8')
    sdk_bin = root / 'tools/msys64/usr/bin'
    make = sdk_bin / 'make.exe'
    shell = sdk_bin / 'sh.exe'
    if not make.is_file() or not shell.is_file():
        parser.error('The SDK bundled Windows make.exe and sh.exe are required')
    dependency = '$(CUSTOM_COBJS): $(TIRTC_AEC_STAMP)'
    assert source.splitlines().count(dependency) == 1
    fragment = '\n'.join(declaration(source, name) for name in ('TIRTC_AEC_STAMP', 'TIRTC_AEC_STAMP_UPDATE'))
    guard_default = next(line for line in source.splitlines() if line.startswith('TIRTC_AUDIO_FREE_GUARD_EN ?='))
    makefile = ('SHELL := ' + shell.as_posix() + '\n'
                'BUILDDIR := out\n'
                'TIRTC_AUDIO_AEC_EN ?= y\n'
                'TIRTC_AUDIO_AEC_DELAY ?= 10\n' + guard_default + '\n' + fragment + '\n'
                '.DEFAULT_GOAL := all\n'
                'CUSTOM_COBJS := $(BUILDDIR)/consumer.o\n' + dependency + '\n'
                '.PHONY: all\n'
                'all: $(CUSTOM_COBJS)\n'
                '$(CUSTOM_COBJS):\n'
                '\t@printf "rebuilt\\n" >> "$(BUILDDIR)/consumer.log"\n'
                '\t@printf "%s\\n" "$(TIRTC_AUDIO_AEC_EN) $(TIRTC_AUDIO_AEC_DELAY) $(TIRTC_AUDIO_FREE_GUARD_EN)" > "$@"\n')
    # Do not let a separately installed Git/Cygwin supply missing SDK tools.
    env = os.environ.copy()
    env['PATH'] = str(sdk_bin) + os.pathsep + str(Path(os.environ.get('SystemRoot', 'C:/Windows')) / 'System32')
    for key in ('MAKEFLAGS', 'MFLAGS', 'MAKEFILES', 'MAKELEVEL', 'SHELLOPTS', 'BASH_ENV', 'ENV'):
        env.pop(key, None)
    report = {'source': str(source_path), 'sha256': hashlib.sha256(source_path.read_bytes()).hexdigest(),
              'make': str(make), 'shell': str(shell), 'cases': [], 'success': False}
    error = None
    try:
        with tempfile.TemporaryDirectory(prefix='pro-aec-stamp-') as temp:
            work = Path(temp)
            (work / 'Makefile').write_text(makefile, encoding='utf-8', newline='\n')
            stamp = work / 'out/pro_aec_config.h'
            consumer = work / 'out/consumer.o'
            log = work / 'out/consumer.log'
            rebuilds = 0
            for label, enabled, delay, guard, change in (
                ('first-create', 'y', '10', None, True),
                ('same-y10', 'y', '10', None, False),
                ('disable', 'n', '10', None, True),
                ('same-n10', 'n', '10', None, False),
                ('delay-change', 'n', '11', None, True),
                ('enable', 'y', '11', None, True),
                ('same-y11', 'y', '11', None, False),
                ('guard-disable', 'y', '11', 'n', True),
                ('same-guard-disabled', 'y', '11', 'n', False),
                ('guard-enable', 'y', '11', 'y', True),
            ):
                previous = None
                if stamp.exists():
                    # Give stamp < consumer < now distinct timestamps without
                    # sleeping or relying on the host filesystem's resolution.
                    past = int(time.time()) - 120
                    os.utime(stamp, (past, past))
                    os.utime(consumer, (past + 60, past + 60))
                    previous = (stamp.stat().st_mtime_ns, consumer.stat().st_mtime_ns)
                command = [str(make), '--no-print-directory', '--silent',
                                         'TIRTC_AUDIO_AEC_EN=' + enabled,
                                         'TIRTC_AUDIO_AEC_DELAY=' + delay]
                if guard is not None:
                    command.append('TIRTC_AUDIO_FREE_GUARD_EN=' + guard)
                result = subprocess.run(command,
                                        cwd=work, env=env, text=True, capture_output=True, timeout=15)
                actual_rebuilds = len(log.read_text().splitlines()) if log.exists() else 0
                current = ((stamp.stat().st_mtime_ns, consumer.stat().st_mtime_ns)
                           if stamp.exists() and consumer.exists() else None)
                report['cases'].append(dict(case=label, returncode=result.returncode, previous_mtime_ns=previous,
                                            current_mtime_ns=current, rebuild_count=actual_rebuilds,
                                            stdout=result.stdout, stderr=result.stderr))
                if result.stderr:
                    print(result.stderr, end='', file=sys.stderr)
                assert result.returncode == 0, label + ': bundled make failed'
                expected_guard = enabled if guard is None else guard
                assert stamp.read_bytes() == (enabled + ' ' + delay + ' ' + expected_guard + '\n').encode(), label + ': wrong stamp content'
                rebuilds += int(change)
                assert actual_rebuilds == rebuilds, label + ': unchanged config unexpectedly rebuilt the consumer'
                assert consumer.read_bytes() == stamp.read_bytes(), label + ': changed flags did not reach the consumer'
                if previous is not None:
                    assert (current != previous) == change, label + ': wrong stamp/consumer timestamp behavior'
                    if change:
                        assert current[0] > previous[0] and current[1] > previous[1], label + ': changed config did not update both files'
                print('PASS:', label, 'rebuilds=' + str(actual_rebuilds))
            report['success'] = True
    except (AssertionError, subprocess.SubprocessError) as failure:
        error = str(failure)
        report['error'] = error
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(report, indent=2), encoding='utf-8')
    if error:
        raise SystemExit('FAIL: ' + error)
    print('PASS: 10 Pro AEC/guard stamp cases using production declarations and SDK tools')


if __name__ == '__main__':
    main()
