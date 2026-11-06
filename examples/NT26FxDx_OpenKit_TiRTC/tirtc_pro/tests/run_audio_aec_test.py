#!/usr/bin/env python3
"""Run Pro's production AEC HAL with deterministic SDK/RTOS boundary fakes.

Each scenario is a fresh process. Non-AEC behavior is covered separately by
the existing run_audio_group_gain_test.py and run_audio_warmup_test.py.
"""
import argparse
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile

SCENARIOS = ("capture", "capture-timeout", "inflight", "gain", "mute", "mic-levels",
             "warm-contract", "backpressure", "pacing", "stop-restart",
             "slow-stop", "stop-timeout", "record-stop-failure", "stream-failure",
             "volume-1", "volume-40", "volume-120", "volume-read-retry", "volume-contention",
             "stale-owner", "init-failure", "startup-timeout")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cc", default=os.environ.get("CC", "cc"))
    parser.add_argument("--scenario", action="append", choices=SCENARIOS)
    args = parser.parse_args()
    executable = shutil.which(args.cc)
    cc = [executable] if executable else [part.strip('"') for part in shlex.split(args.cc, posix=os.name != "nt")]
    if not cc or not shutil.which(cc[0]):
        parser.error("C compiler unavailable; pass --cc")
    app = Path(__file__).resolve().parents[1]
    with tempfile.TemporaryDirectory(prefix="pro-aec-hal-") as temp:
        for group in (False, True):
            exe = Path(temp) / ("aec-group.exe" if group else "aec-basic.exe")
            flags = ["-std=c99", "-O2", "-Wall", "-Wextra", "-Werror",
                     "-DDEMO_AI_AUDIO_AEC_ENABLE=1", "-DDEMO_AI_AUDIO_AEC_TEST"]
            if group:
                flags.append("-DHWDEMO_GROUP_ROOM_EN")
            flags += ["-I" + str(app / "tests/audio_aec_host_stubs"),
                      "-I" + str(app / "tests/audio_gain_host_stubs"),
                      "-I" + str(app / "media"),
                      "-I" + str(app.parents[2] / "components/kernel/lierda_api/liot_audio2"),
                      str(app / "tests/audio_aec_test.c"), str(app / "media/audio_device.c"),
                      "-o", str(exe)]
            subprocess.run(cc + flags, check=True)
            for scenario in args.scenario or SCENARIOS:
                subprocess.run([str(exe), scenario], check=True, timeout=15)
    print("PASS: Pro AEC HAL scenarios with Group disabled/enabled")


if __name__ == "__main__":
    main()
