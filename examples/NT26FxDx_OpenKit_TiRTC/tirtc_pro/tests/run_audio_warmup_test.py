#!/usr/bin/env python3
"""Compile the real HAL and test single-item warmup failure accounting."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cc', default='zig', help='Zig, clang or GCC executable')
    parser.add_argument('--report', type=Path, help='Optional JSON result file')
    parser.add_argument('--check-old-chunk', action='store_true', help='Verify a temporary300ms mutation fails')
    args = parser.parse_args()
    cc = shutil.which(args.cc)
    if not cc:
        parser.error('Compiler not found; pass --cc with its executable path')
    app = Path(__file__).resolve().parents[1]
    source = app / 'media/audio_device.c'
    outputs, commands = [], []
    mutation_caught = None
    with tempfile.TemporaryDirectory(prefix='pro-audio-warmup-') as output:
        output = Path(output)
        for room in (False, True):
            exe = output / ('warm-room.exe' if room else 'warm-original.exe')
            command = [cc] + (['cc'] if Path(cc).stem == 'zig' else [])
            command += ['-std=c11', '-O2', '-Wall', '-Wextra', '-Werror']
            if room:
                command.append('-DHWDEMO_GROUP_ROOM_EN')
            command += ['-I' + str(app / 'tests/audio_gain_host_stubs'),
                        '-I' + str(app.parents[2] / 'components/kernel/lierda_api/liot_audio2'),
                        str(app / 'tests/audio_warmup_test.c'), str(source), '-o', str(exe)]
            commands.append(command)
            subprocess.run(command, check=True)
            run = subprocess.run([str(exe)], capture_output=True, text=True, check=True)
            outputs.append(run.stdout)
            print(run.stdout, end='')
        if args.check_old_chunk:
            original = source.read_text()
            assert original.count('#define SESSION_CHUNK_MS 150U') == 1
            mutant = output / 'audio_device.c'
            # Absolute includes keep the production file otherwise intact.
            mutant.write_text(original.replace('#define SESSION_CHUNK_MS 150U', '#define SESSION_CHUNK_MS 300U')
                .replace('#include "audio_device.h"', '#include "' + (app / 'media/audio_device.h').as_posix() + '"')
                .replace('#include "../tirtc_log.h"', '#include "' + (app / 'tirtc_log.h').as_posix() + '"'))
            exe = output / 'old-chunk.exe'
            mutant_command = [str(mutant) if x == str(source) else x for x in commands[-1]]
            mutant_command[-1] = str(exe)
            subprocess.run(mutant_command, check=True)
            run = subprocess.run([str(exe)], capture_output=True, text=True)
            mutation_caught = run.returncode != 0
            assert mutation_caught, 'Old partial-prefix defect was not detected'
            print('PASS: old300ms/9600B mutation rejected by SDK item2 failure regression')
    report = {'source': str(source), 'source_sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
              'commands': commands, 'outputs': outputs, 'old_chunk_mutation_caught': mutation_caught,
              'boundary': 'Real HAL; fake SDK uses5120-byte independently accepted items', 'serial_access': False}
    if args.report:
        args.report.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')


if __name__ == '__main__':
    main()
