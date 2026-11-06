#!/usr/bin/env python3
"""Compile the actual call TX/lifecycle functions with a deterministic scheduler.

Usage: python run_calls_audio_lifecycle_test.py --cc /path/to/zig
The compiler may be Zig, GCC, Clang or TinyCC; no SDK binary is required.
"""
import argparse
from pathlib import Path
import re
import shlex
import subprocess
import tempfile


def function(source, name):
    match = re.search(r"(?m)^(?:static\s+)?(?:void|int|bool|uint8_t|uint32_t)\s+"
                      + re.escape(name) + r"\s*\(", source)
    if not match:
        raise ValueError("Missing production function: " + name)
    opening = source.index("{", match.start())
    depth = 0
    for token in re.finditer(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|[{}]',
                             source[opening:], re.S):
        if token.group() == "{":
            depth += 1
        elif token.group() == "}":
            depth -= 1
            if not depth:
                return source[match.start():opening + token.end()]
    raise ValueError("Unclosed production function: " + name)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cc", default="cc", help="C compiler executable")
    parser.add_argument("--cflags", default="", help="Additional compiler options")
    args = parser.parse_args()
    tests = Path(__file__).resolve().parent
    app = tests.parent
    source = (app / "calls/calls_audio.c").read_text(encoding="utf-8")
    declarations = source[source.index("#define RX_BYTES"):source.index("static uint32_t now(void)")]
    names = ["now", "before", "audio_error", "failure_log", "fatal_error",
             "audio_session_locked", "speaker_allowed_locked", "microphone_allowed_locked",
             "effective_mic_level_locked", "dev_diagnostic_tx_locked", "dev_diagnostic_report",
             "revise_config_locked", "discard_rx", "tx_job_current_locked", "tx_send_job",
             "tx_worker", "calls_audio_start_service", "calls_audio_start", "calls_audio_stop"]
    actual = "\n".join(function(source, name) for name in names)
    template = (tests / "calls_audio_lifecycle_test.c").read_text(encoding="utf-8")
    generated = template.replace("/* PRODUCTION_DECLARATIONS */", declarations).replace(
        "/* PRODUCTION_FUNCTIONS */", actual)
    with tempfile.TemporaryDirectory(prefix="pro-calls-lifecycle-") as temporary:
        directory = Path(temporary)
        unit, binary = directory / "test.c", directory / "test.exe"
        unit.write_text(generated, encoding="utf-8")
        command = [args.cc]
        if Path(args.cc).stem.lower() == "zig":
            command.append("cc")
        command += ["-std=c99", "-O0", *shlex.split(args.cflags), str(unit), "-o", str(binary)]
        subprocess.run(command, check=True)
        subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    main()
