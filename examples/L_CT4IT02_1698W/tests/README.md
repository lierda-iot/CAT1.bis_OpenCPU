# L_CT4IT02_1698W tests

This directory is the project-level entry for the first-stage host test line.

- `make -C examples/L_CT4IT02_1698W/tests host-test` runs host Unity tests.
- `make -C examples/L_CT4IT02_1698W/tests host-test-smoke` runs one smoke case
  per module.
- `make -C examples/L_CT4IT02_1698W/tests list-tests` lists host test cases.
- Module tests are co-located in each production seam's `test/` subdirectory
  (for example `driver/src/lvgl/watch_earth_2015/test`,
  `demo/src/demo_mp3/test`); see `docs/SUMMARY.md` §4 for per-module paths.

The first stage intentionally focuses on a small set of high-value cases instead
of broad smoke coverage.

## 1. Adding a new module to the host test line

1. Add the module's test directory to `MODULE_MAP` in `tests/Makefile`.
2. Co-locate Unity tests next to the production seam, for example
   `driver/src/lvgl/baji_photo/test/` or `demo/src/demo_mp3/test/`.
3. Keep the module test target self-contained: `test`, `smoke-test` and
   `list-tests` so the project-level wrapper can drive it unchanged.

Do not add per-module registry files or fixture directories until the unified
gate entry itself is re-implemented, otherwise they stay dead code.
