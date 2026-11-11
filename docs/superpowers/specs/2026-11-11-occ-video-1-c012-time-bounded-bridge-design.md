# occ_video_1 C012 Time-Bounded Bridge Design

C011 showed that a fixed frame budget is path-dependent: allowing two bridge
frames moved the next score-only anchor dip to frame 202; a diagnostic three-
frame budget moved it to 203. Incrementing the frame count would overfit the
video rather than model the failure.

C012 replaces the consecutive-frame budget with
`template_bridge_max_duration_ms`. A mature dual-template streak arms the same
strict score-only bridge gates, but continuation is bounded by elapsed time from
the first bridge observation. The visible candidate uses 200 ms, a short
response-hysteresis interval independent of frame rate. The default is zero and
historical configurations remain disabled through
`template_bridge_min_stable_frames=0`.

The bridge ends immediately when dual-template reliability returns. It also
ends on any rejection, search, reacquisition, probation, detector verification,
occlusion trigger, missing template, or geometry disagreement. If the strict
score-only condition persists beyond 200 ms, ordinary search begins. Reliability
and evaluator thresholds are unchanged.
