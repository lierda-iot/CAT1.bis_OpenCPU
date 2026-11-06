#!/usr/bin/env python3
"""Check actual SDK make rules when Pro headers change, without compiling firmware.

Only temporary object placeholders and the build stamp are written. make -W
simulates changed inputs; no production source timestamps/content are changed.
"""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile


def main():
    app = Path(__file__).resolve().parents[1]
    root = app.parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--gcc-path', type=Path, default=root / 'tools/toolchain/gcc')
    parser.add_argument('--report', type=Path)
    parser.add_argument('--without-header-dependency', action='store_true',
                        help='Negative control: disable the product include prerequisites')
    args = parser.parse_args()
    sdk_bin = root / 'tools/msys64/usr/bin'
    make = sdk_bin / 'make.exe'
    if not make.is_file() or not (args.gcc_path / 'bin/arm-none-eabi-gcc.exe').is_file():
        parser.error('SDK Windows make and an extracted ARM GCC are required')
    env = os.environ.copy()
    env['PATH'] = str(sdk_bin) + os.pathsep + env.get('PATH', '')
    for key in ('MAKEFLAGS', 'MFLAGS', 'MAKEFILES', 'MAKELEVEL', 'SHELLOPTS', 'BASH_ENV', 'ENV'):
        env.pop(key, None)
    cases = (
        ('unchanged', None, False, 'app'),
        ('audio-header', app / 'media/audio_device.h', True, 'app'),
        ('logging-header', app / 'tirtc_log.h', True, 'app'),
        ('nested-sdk-header', app / 'sdk/include/tirtc/tiRTC.h', True, 'app'),
        ('aec-include', app / 'media/audio_aec.inc', True, 'app'),
        ('host-test-header', app / 'tests/audio_aec_host_stubs/liot_os.h', False, 'app'),
        ('source', app / 'media/audio_device.c', True, 'app'),
        ('app-lvgl-config', app / 'ui/lv_conf.h', True, 'app'),
        ('lvgl-unchanged', None, False, 'lvgl'),
        ('lvgl-product-config', app / 'ui/lv_conf.h', True, 'lvgl'),
        ('lvgl-unrelated-header', app / 'tirtc_log.h', False, 'lvgl'),
        ('lvgl-source', root / 'components/thirdparty/lvgl/lvgl-8.3.10/src/misc/lv_mem.c', True, 'lvgl'),
    )
    results = []
    with tempfile.TemporaryDirectory(prefix='pro-include-deps-') as temporary:
        build = Path(temporary)
        (build / 'pro_aec_config.h').write_bytes(b'y 10\n')
        stems = {'app': build / app.relative_to(root) / 'media/audio_device',
                 'lvgl': build / 'components/thirdparty/lvgl/lvgl-8.3.10/src/misc/lv_mem'}
        targets = {}
        for kind, stem in stems.items():
            stem.parent.mkdir(parents=True)
            targets[kind] = [stem.with_suffix('.o'), stem.with_suffix('.pp')]
            for target in targets[kind]:
                target.write_text('dependency test placeholder\n', encoding='ascii')
        command = [str(make), '--no-print-directory', '-n', '-f', 'Makefile',
                   'TOP=' + root.as_posix(), 'BUILDDIR=' + build.as_posix(),
                   'GCC_PATH=' + args.gcc_path.resolve().as_posix(),
                   'PROJECT=NT26FxDx_OpenKit', 'APP_VARIANT=pro', 'BUILD_MODE=tirtc',
                   'TIRTC_ASSET_INSTALLER=n', 'MODEM=NT26F6D0', 'MODEMPKG=F6D_D',
                   'BINNAME=TiRTC_pro_NT26F6D0']
        if args.without_header_dependency:
            command.append('TIRTC_PRO_INCLUDES=')
        for name, source, expected, kind in cases:
            if source is not None and not source.is_file():
                raise AssertionError('Missing input: ' + str(source))
            changed = [] if source is None else ['-W', source.as_posix()]
            result = subprocess.run(command + changed + [p.as_posix() for p in targets[kind]],
                                    cwd=root / 'rules', env=env, capture_output=True,
                                    text=True, errors='replace', timeout=45)
            obj_rebuild = ' -c ' in result.stdout
            pp_rebuild = ' -E ' in result.stdout
            passed = result.returncode == 0 and obj_rebuild == expected and pp_rebuild == expected
            results.append(dict(case=name, passed=passed, object_rebuild=obj_rebuild,
                                preprocessing_rebuild=pp_rebuild, expected=expected,
                                returncode=result.returncode, stderr=result.stderr,
                                stdout=result.stdout if not passed else ''))
            print(('PASS: ' if passed else 'FAIL: ') + name)
    if args.report:
        args.report.write_text(json.dumps(results, indent=2), encoding='utf-8')
    if not all(case['passed'] for case in results):
        raise SystemExit('FAIL: product include dependencies did not match changes')
    print('PASS: 12 Pro include dependency cases using actual SDK make rules')


if __name__ == '__main__':
    main()
