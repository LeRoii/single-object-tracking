# RK3588 A8 isolated human-test candidate v2

Source: `codex/rk3588-display-box-stability@ba465d4b17bba2946e82fafdf283f6c911293ea8`.
The A8 adapter/diagnostic template was consumed from
`codex/rk3588-a8-cross-compile@3fffd207a62dfc87a25f3864311a9f2173663d77`.
The unchanged A8 `libviewlink.a` SHA-256 is
`fec1c4326051446a90890132b03834d89ba9763a73237f34adcb09904032c040`.

This candidate changes UNKNOWN response entry to three distinct failed metrics,
uses that same threshold for reliable recovery, pauses template learning on any
failed metric, and bounds display-box catch-up after motion or SUSPECTED.
Tracking/control `rect[0]` and the A8 host serial code are unchanged.
The controller config SHA-256 is
`4dafcabf08a7ae68c79ca9895f4b382583bd214397829dfeb60ba9f511f966f7`.
`controller-off.conf` is an explicit two-vote/display-stability-off comparison
config; the default launcher uses only `controller.conf`.

The WSL build command was `bash build-wsl.sh` in
`/home/wjm/a8-display-stability-v2-20261120`, which invokes
`BUILD_JOBS=4 ./run.sh --platform A8`. Input checksums are in
`build-inputs.sha256` (SHA-256
`75b1501ac80f71171c5c3c3acc9232747dc45988391dfb24a132ca734b9deb03`).
WSL build log SHA-256 was
`cb27c9555430e5696cb8f0862604efaf3e8e4de199d3aeb47820c4b4d41199a7`;
restoration check log SHA-256 was
`b0ce570cbe87ad1060b1cdbe71a06e95e74fcad7c681f35e89cb5ed01381b7cb`.
The AArch64 `viewlink_app` SHA-256 is
`a7cdf62095fdcdeeafb107988c509d85055b7ed816ca48b85541d9f772d54c43`.

Installed at `/app/nanotrack_rk3588_display_stability_v2_20261120`.
The transport tar SHA-256 is
`3f0860665eac32b9c9f186b350cb767a2bf98fcc8118e8f6fe035f0707be3a7f`.
The pod copied the seven checked models and private launch files internally
from the prior isolated candidate; they were not exported. `payload.sha256`
(SHA-256 `231ec279519c950d754fc0cf80f8095f9c8f716508f9a9ba6acd9c7a40fb66f1`)
checks 17 entries; `models.sha256` checks seven. Both passed on the pod.
The original `/app/bin/viewlink_app` SHA-256 remained
`46325de3d9ba8a5850af48b91c0189ee52a993e233c59858578099efc94a1e70`.

`sh start-test.sh --check` passed with `AI_CONTROL_LIMIT=0`,
`AI_DEBUG_IMAGES=0`; it did not start the app. Preflight logs are under
`log/preflight-limit-0-2485-5000/`; input and video check SHA-256 values:
`51ea197765589a6504fd287cb09cf9f5eebe98320dcaed298026bc1a801946a2`
and `fe08d7756877e246ff904c5591d5b5d8e6530c86c44ed29d977244a038b7118f`.
No app process was running afterward. Manual commands on the pod:

```sh
sh /app/nanotrack_rk3588_display_stability_v2_20261120/start-test.sh
sh /app/nanotrack_rk3588_display_stability_v2_20261120/stop-test.sh
```

The startup output prints the per-run log directory. This is an isolated
human-test candidate. No new RK3588 native validation or pod click-tracking
result has been claimed; the shared production handoff remains unavailable.
