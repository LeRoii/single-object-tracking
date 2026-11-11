# occ_video_1 C012 Time-Bounded Bridge Plan

- Replace the C011 frame-count setting/state/metadata with a millisecond
  duration, valid `0..1000`, and set the visible candidate to `200`.
- Test acceptance through the exact duration boundary and rejection immediately
  after it, plus all existing no-history, geometry, probation, reset, identity,
  metadata, and verifier contracts.
- Run 19 CTests and task-relevant Python tests, commit, and package the exact
  source for RK3588 Debug/RKNN verification.
- Run the complete 271-frame video once. Repeat twice only after verifier and
  every full evaluator gate pass. Record post-holdout evidence and do not claim
  real-gimbal acceptance.
