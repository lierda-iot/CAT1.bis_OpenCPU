#!/usr/bin/env python3
"""Run the shared Stop ownership regression against the actual Pro adapter.

Pro retains its own runtime/header and v2.5.0-9088239c archive. The native
Stop/Uninit/close paths match the audited Lite contract, so only the host fake
and common cases are shared. Pro-specific service leases and LIVE/standby
state are exercised here. No radio timing or SDK callback-join guarantee is
inferred from these tests; the during-Stop window remains an explicit limit.
"""
import argparse
import importlib.util
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
PRODUCT = Path(__file__).resolve().parents[1]
LITE_TESTS = PRODUCT.parent / 'tirtc_lite/tests'
sys.path.insert(0, str(LITE_TESTS))
spec = importlib.util.spec_from_file_location(
    'lite_stop_gate_shared', LITE_TESTS / 'run_tirtc_stop_gate_test.py')
shared = importlib.util.module_from_spec(spec)
spec.loader.exec_module(shared)


def replace_once(text, old, new):
    if text.count(old) != 1:
        raise RuntimeError('Shared harness anchor changed: ' + old)
    return text.replace(old, new, 1)


PRO_CASES = r'''
    reset();mode=3;
    assert(shutdown_probe()==TIRTC_E_BUSY&&injections==1&&!stops&&!uninits);
    assert(s_service_users==1U&&!s_stop_requested);
    s_service_users=0;mode=0;
    assert(shutdown_probe()==0&&stops==1&&uninits==1);
    pass("Pro late service lease blocks first Stop until the lease is released");
    reset();s_service_users=1U;
    assert(shutdown_probe()==DEMO_TIRTC_ERR_TIMEOUT&&!stops&&!uninits);
    assert(s_service_users==1U&&!s_stop_requested);
    s_service_users=0;
    assert(shutdown_probe()==0&&stops==1&&uninits==1);
    pass("Pro preexisting service lease preserves the original drain predicate");
    reset();s_connection.active=HANDLE;s_connection.active_owner=DEMO_TIRTC_OWNER_LIVE;
    s_connection.owner=DEMO_TIRTC_OWNER_LIVE;s_connection.session_generation=9;
    s_connection.active_generation=9;
    assert(shutdown_probe()==0&&disconnects==1&&stops==1&&uninits==1&&!unsafe_uninits);
    pass("Pro LIVE owner drains by the existing handle release path");
    reset();s_ai_standby_owned=s_ai_standby_revoked=true;
    s_connection.owner=DEMO_TIRTC_OWNER_AI;s_connection.session_generation=7;
    assert(shutdown_probe()==0);
    runtime_reset_after_uninit();
    assert(!s_ai_standby_owned&&!s_ai_standby_revoked&&!s_runtime_route_valid);
    assert(s_retired_session_generation[DEMO_TIRTC_OWNER_AI]==7);
    assert(s_connection.next_session_generation!=0U);
    pass("Pro reset retains standby, route and retired-generation cleanup");
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cc', default=os.environ.get('CC', 'cc'))
    parser.add_argument('--before', type=Path,
                        help='original Pro runtime C: verify the known negative control')
    args = parser.parse_args()
    cc = shared.compiler_command(args.cc, parser)
    source_path = args.before or PRODUCT / 'runtime/tirtc_runtime.c'
    source = source_path.read_text(encoding='utf-8')
    constants = '\n'.join(re.findall(r'^#define DEMO_TIRTC_\w+[^\n]*', source, re.M))
    constants = '\n'.join(line for line in constants.splitlines() if not line.endswith('\\'))
    declarations = source[source.index('static const demo_tirtc_listener_t * volatile'):
                          source.index('static bool runtime_dev_connect_needs_cleanup')]
    functions = []
    for name in shared.NAMES:
        if name == 'runtime_connections_drained_locked' and name not in source:
            continue
        functions.append(shared.function(source, name))

    prefix = replace_once(shared.PREFIX, 'static unsigned clock_ms,',
                          'static bool s_runtime_route_valid;\nstatic void inject_service(void);\n'
                          'static unsigned clock_ms,')
    prefix = replace_once(prefix, 'if (mode==1U && entries==5U) inject_accept();',
                          'if (mode==1U && entries==5U) inject_accept();\n'
                          '        if (mode==3U && entries==5U) inject_service();')
    suffix = replace_once(shared.SUFFIX, 'static void inject_accept(void) {',
                          'static void inject_service(void) {\n'
                          '    assert(!critical_depth);++injections;s_service_users=1U;\n'
                          '}\nstatic void inject_accept(void) {')
    suffix = replace_once(suffix, '    s_reject_pending=s_accept_callbacks_pending=0;',
                          '    s_reject_pending=s_accept_callbacks_pending=0;\n'
                          '    s_service_users=0;s_runtime_route_valid=true;\n'
                          '    s_ai_standby_owned=s_ai_standby_revoked=false;')
    suffix = replace_once(suffix, '    reset();mode=2;', PRO_CASES + '\n    reset();mode=2;')
    unit = prefix + '\n' + constants + '\n' + declarations + '\n' + '\n'.join(functions) + suffix
    with tempfile.TemporaryDirectory(prefix='pro-tirtc-stop-gate-') as output:
        code = Path(output) / 'stop_gate.c'
        exe = Path(output) / 'stop_gate.exe'
        code.write_text(unit, encoding='utf-8')
        cmd = cc + ['-std=c99', '-O1', '-UWIN32', '-DHWDEMO_GROUP_ROOM_EN']
        if args.before:
            cmd.append('-DEXPECT_OLD_GAP')
        cmd += ['-I', str(PRODUCT / 'runtime'), '-I', str(PRODUCT / 'sdk/include'),
                str(code), '-o', str(exe)]
        subprocess.run(cmd, check=True)
        subprocess.run([str(exe)], check=True)


if __name__ == '__main__':
    main()
