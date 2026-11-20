# RK3588 A8 display stability human-test candidate

This isolated candidate contains the display-box algorithm from
`9253f4b6cf0edec3e4a9fe9674bb976ed59d7295` on branch
`codex/rk3588-display-box-stability` (checkout before packaging
`f4c6ef8a151fa43c9720f18fbd604d77c6dbc625`). The A8 adapter and
diagnostic wrapper were consumed from
`codex/rk3588-a8-cross-compile@3fffd207a62dfc87a25f3864311a9f2173663d77`.
The consumed adapter template SHA-256 was
`0d0078aea3984f5f4abf67fbdd435745cff0dbd391e0cc55b653958fb41d19ab`;
only the deployment path and version string were changed. It uses the existing
A8 `libviewlink.a` SHA-256
`fec1c4326051446a90890132b03834d89ba9763a73237f34adcb09904032c040`.

The WSL input source manifest is `build-inputs.sha256`, generated from the
committed `rk3566/nanotrack_v3/{src,include}` files plus the adjacent A8
adapter, limiter and diagnostic files. `build-wsl.sh` backs up and restores
the WSL source and build files. Actual command was `bash build-wsl.sh` in
`/home/wjm/a8-display-stability-20261120`, which invokes
`BUILD_JOBS=4 ./run.sh --platform A8`. The WSL project is
`/home/wjm/viewpro_gimbal_zp/rk3566/gimbal_tracker`.

The resulting `viewlink_app` is AArch64 ELF, SHA-256
`625e62203700a4cfd5d52dd529f38631aa2624fed0e9b3201e317a709f3cedec`.
The input/controller/production configuration SHA-256 values are in
`build-inputs.sha256` and `payload.sha256`; the enabled controller config is
`3411ec9f9c19a8666e117e4ac82d8084f2b7a118e8850ea0b60c08c1277bca67`.
The `controller-off.conf` file is a comparison config and is not used by the
default launcher. `AI_CONTROL_LIMIT=0` and `AI_DEBUG_IMAGES=0` by default.

Pod installation: `/app/nanotrack_rk3588_display_stability_20261120`.
Seven model files, the private video launcher and the system configuration
seed were copied **within the pod** from the previous isolated candidate;
their contents were not downloaded. The complete installed payload passed
`sha256sum -c payload.sha256` (17 entries) and `sha256sum -c models.sha256`
(seven entries). The original `/app/bin/viewlink_app` was not modified.
`sh start-test.sh --check` passed the private mount and model preflight;
the application was not started.

Manual commands on the pod:

```sh
sh /app/nanotrack_rk3588_display_stability_20261120/start-test.sh
# After the test:
sh /app/nanotrack_rk3588_display_stability_20261120/stop-test.sh
```

The start script prints the run log directory under
`/app/nanotrack_rk3588_display_stability_20261120/log/`. The source remains
an isolated human-test candidate. Static video comparison was accepted by
the user; real pod tracking behavior awaits this manual test. The shared
`integration-contract.yaml` still has no approved production handoff.
