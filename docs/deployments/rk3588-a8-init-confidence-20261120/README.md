# RK3588 A8 initialization detector confidence candidate

Source: `codex/rk3588-display-box-stability@a2025d3c8c8076728b3731c1fd72690ec52ffaac`.
The A8 adapter/diagnostic template was consumed from
`codex/rk3588-a8-cross-compile@3fffd207a62dfc87a25f3864311a9f2173663d77`.
The unchanged A8 `libviewlink.a` SHA-256 is
`fec1c4326051446a90890132b03834d89ba9763a73237f34adcb09904032c040`.

Initialization candidates must have detector confidence strictly greater than
0.5 before click ROI geometry matching. A lower-confidence candidate is kept
for diagnostics with `LOW_CONFIDENCE`, but cannot establish class or select a
large detector display box. The NanoTrack initial box is still the clicked ROI.
The detector engine's 0.25 inference threshold and recovery logic are unchanged.
This A8 adapter additionally writes `AI_INIT_CANDIDATE` lines containing each
candidate's original class, bbox, confidence and rejection; `AI_INIT_CLASS`
records the result. These are in each run's `stderr.log`.

The WSL command was `bash build-wsl.sh` in
`/home/wjm/a8-init-confidence-20261120`, internally invoking
`BUILD_JOBS=4 ./run.sh --platform A8`. Input manifest
`build-inputs.sha256` has SHA-256
`05a5858a7f9299a79e56913aec2a59344064cb0fdb2eb2dba836c57ed0c5721f`.
Build log SHA-256 is
`b618e5254b69259dce9afa6131cd57f5393fa892e237048646b3712cbe4a6305`;
WSL restoration log SHA-256 is
`b0ce570cbe87ad1060b1cdbe71a06e95e74fcad7c681f35e89cb5ed01381b7cb`.
AArch64 `viewlink_app` SHA-256 is
`7f31a223d638253d69ed5d0be0ae2819c321419725872ed271345de54124672b`.

Installed at `/app/nanotrack_rk3588_init_confidence_20261120`.
Transport tar SHA-256 is
`8608b4b8aacdaefb5796436f3b534a145323b5f0c40f0e00817f2c3b97486c16`.
Seven verified models and private launch files were copied internally on the
pod from the preceding isolated candidate. The `payload.sha256` file has
SHA-256 `f42cad4f9fbcc3adc812d3ebde584e19af0c96cff959d24b92a7a2c9d56d3276`;
its 17 entries and the seven model hashes passed on the pod. The original
`/app/bin/viewlink_app` remained SHA-256
`46325de3d9ba8a5850af48b91c0189ee52a993e233c59858578099efc94a1e70`.

`sh start-test.sh --check` passed without starting the app. Preflight log:
`log/preflight-limit-0-3853-11103/`, input/video check hashes:
`51ea197765589a6504fd287cb09cf9f5eebe98320dcaed298026bc1a801946a2` /
`fe59766ad4a5a5881dbf1704c7b7c7e40ae04a075581bfade961ee107e822c3a`.
`AI_CONTROL_LIMIT=0` and `AI_DEBUG_IMAGES=0` remain the defaults; no app process
was running afterward. Manual pod commands:

```sh
sh /app/nanotrack_rk3588_init_confidence_20261120/start-test.sh
sh /app/nanotrack_rk3588_init_confidence_20261120/stop-test.sh
```

The launcher prints each run's log directory. This is an isolated human-test
candidate. No new development-board or click-tracking result is claimed.
The shared production handoff remains unavailable.
