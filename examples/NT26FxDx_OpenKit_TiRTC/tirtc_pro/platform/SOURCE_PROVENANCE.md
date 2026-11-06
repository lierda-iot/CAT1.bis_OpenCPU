# Platform source provenance

Upstream: [tangeai/xiaotai-lierda](https://github.com/tangeai/xiaotai-lierda/tree/f159bfd3e82cc514dfe6b5b2269b2c96fa8e8f93/examples/L_CT4IT00_YP00W_01_V04/tirtc), fixed commit `f159bfd3e82cc514dfe6b5b2269b2c96fa8e8f93`.

| Local file | Upstream path | Adaptation |
| --- | --- | --- |
| `device_binding.c` | `src/device_binding.c` | Preserve discovery/report/grant/NVM/token flow; remove unrelated AI request API and hardware power/reboot behavior; add network epochs, bounded UI snapshots, real SIM guard and verified cleanup. |
| `device_binding.h` | `include/device_binding.h` | Narrow public identity, worker and snapshot contract; remove unrelated AI API. |
| `formal_mqtt.c` | `src/formal_mqtt.c` | Preserve core MQTT transport and routing; remove incompatible private RX buffer patch; fix reconnect receive gate; verify actual async completion and serialize shared pool reuse. |
| `formal_mqtt.h` | `include/formal_mqtt.h` | Preserve router interface; add lifecycle-owner cleanup gate. |
| `json_guard.h` | `include/json_guard.h` | Preserve bounded-depth parsing guard. |

Original copyright and provenance notices remain in adapted files. On 2026-09-28, at the project maintainer's request, Tange-owned adaptations previously marked `MIT AND Apache-2.0` were unified under [MIT](../LICENSE); the existing MIT-covered upstream expressions remain MIT. The historical upstream MIT and Apache-2.0 texts are retained in `LICENSES/`. This change does not relicense vendor or third-party materials; see the [repository license scope](../THIRD_PARTY_NOTICES.md). `tirtc_platform.c/.h` and `platform_internal.h` implement this board's independent UI/network adapter; `tirtc_mqtt_compat.c/.h` is a new, hash-gated compatibility probe for the exact bundled SDK, not a borrowed upstream ABI patch.


