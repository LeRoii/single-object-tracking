# RK3566 Debug Runtime Telemetry Design

## Intent

Improve terminal observability of the interactive RK3566 NanoTrack debug
program so a human can understand startup, playback, tracking state,
detector-assisted recovery, template adaptation, and shutdown without enabling
output files or changing tracking decisions.

## Goals

- Emit one-line structured messages using the prefix `DEBUG_EVENT` and
  `key=value` fields.
- Report startup/video/model/detector/output configuration and interactive
  commands.
- Report tracking-mode and `control_valid` changes immediately.
- Report detector request/completion/error, detector candidate verification,
  same-class ambiguity, and confirmation progress.
- Report reacquisition source and recent-template stage/commit/rollback.
- Emit a frame heartbeat for every manual step and every 30 tracked frames
  during continuous playback.
- Keep terminal logging enabled only for the interactive debug path, except for
  the existing final summary and fatal errors.

## Non-goals and safety constraints

- Do not change controller thresholds, search plans, template decisions,
  detector scheduling, or tracker state.
- A detector result remains a hypothesis. It must never directly set
  `control_valid=true`.
- Do not add persistent logging by default and do not add a new third-party
  dependency.
- Do not print a complete per-frame record during continuous playback.

## Approaches considered

1. State-change-only messages have the least overhead but do not expose timing
   drift or a long period with no transitions.
2. Full per-frame messages provide maximum detail but can materially slow the
   RK3566 program and hide important events in noise.
3. Structured event messages plus a throttled heartbeat provide state-machine
   evidence and periodic liveness/timing data with bounded output. This is the
   selected approach.

## Design

Add a small `DebugTelemetry` component beside `tracking_log`. It consumes the
already-produced `RuntimeFrameResult` and pipeline latency, remembers only the
last published mode/control state and a frame counter, and returns formatted
event lines. The component is observational and has no reference back into the
controller or tracker.

The interactive loop reports user commands and lifecycle events directly. A
manual step forces a heartbeat; continuous playback receives the component's
30-frame heartbeat. Reinitialization resets the telemetry comparison state so
the new target begins with an explicit baseline event.

All fields are single-line and locale-independent. Bounding boxes and floating
metrics use fixed decimal precision. Missing optional detector fields are
written as `none` rather than inferred.

## Verification

- Unit tests prove prefix/field formatting, transition suppression, forced and
  periodic heartbeat behavior, detector diagnostics, reacquisition, template
  events, and reset behavior.
- The full native test suite must pass on the development board.
- A bounded interactive board run must show startup and command/status events;
  it is not evidence of recovery quality or production acceptance.
