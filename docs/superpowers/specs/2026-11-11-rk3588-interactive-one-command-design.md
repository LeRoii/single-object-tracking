# RK3588 Interactive One-Command Configuration Design

## Intent

Make the final accepted RK3588 tracker runnable by a human from
`/home/neardi/Project/SOT/occ-candidate-e865c78` with exactly:

```bash
./build/nanotrack_v3 --config rk3588-interactive-debug.conf
```

The program must still pause on the first video frame and let the user select or
reset the target with the mouse.

## Selected approach

Reuse the existing strict runtime-config loader; do not add another CLI or file
format. Update the shipped `rk3588-interactive-debug.conf` to contain the exact
absolute RK3588 video, tracker-model, detector, accepted C012 controller, output,
frame-range, and target-class parameters. Deploy the same file at the immutable
candidate root named by the command above.

`interactive=true` remains enabled and `bbox` remains absent deliberately: the
mouse selection is runtime user input, not a fixed parameter. Debug artifacts use
fixed `/tmp/rk3588-interactive-debug.*` paths so their parent always exists and a
repeated manual run predictably replaces the prior debug output.

## Alternatives considered

- A new profile would preserve the old file verbatim, but the user explicitly
  chose to modify this profile and use its existing name.
- Changing CLI parsing is unnecessary because `--config` already supports every
  required parameter and command-line overrides.

## Acceptance

The CLI test must parse the shipped profile as interactive/no-bbox, select the
accepted controller and RK3588 detector, and expose the exact output and frame
settings. All host CTests must pass. On RK3588 the exact two-line command must
load the final binary/models/config and reach `interactive_ready`; a bounded
startup smoke does not constitute human ROI or real-gimbal acceptance.
