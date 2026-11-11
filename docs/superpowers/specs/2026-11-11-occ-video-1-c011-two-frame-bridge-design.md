# occ_video_1 C011 Two-Frame Bridge Design

## Evidence

C010 proved that a temporal bridge is the correct mechanism but its one-frame
budget is too short. Frames 200 and 201 are consecutive score-only anchor dips:
the recent template remains reliable, anchor/recent IoU is `0.950447` and
`0.943285`, and all anchor geometry gates pass. At frame 202 both templates are
reliable again. C010 bridges frame 200, enters search at 201, and later emits a
second reacquisition at 217. All full evaluator gates except exactly-one-
reacquisition pass; valid-observation mean IoU is `0.734841`.

The real occlusion transition remains distinguishable: recent-template quality
was already weak during frames 166--168, so no mature consecutive dual-reliable
history is armed when frame 169 degrades.

## Design

Add `template_bridge_max_consecutive_frames` with default `1`, valid range
`1..10`, and C011 visible value `2`. The existing minimum stable streak remains
`3`. Maintain `template_bridge_consecutive_frames_` in addition to the mature
dual-template streak.

A first bridge frame requires the existing mature streak and all C010 gates. A
following bridge frame may continue without rebuilding the streak only when the
same score-only anchor failure, reliable recent observation, template agreement,
ordinary-tracking state, and safety gates remain true, and the consecutive count
is below the configured maximum. Each bridge increments the consecutive count.

An ordinary accepted dual-reliable frame clears the consecutive count and starts
rebuilding the mature streak. Any rejection, search, reacquisition,
initialization, probation, detector verification, occlusion trigger, missing
template, or geometry disagreement clears both states. The third consecutive
score-only failure with C011 configuration must enter search.

## Safety and generalization

The two-frame limit is a generic short hysteresis window, not a frame-ID or
truth-coordinate rule. It changes no score, PSR, APCE, peak-margin, geometry, or
evaluator threshold. It never accepts detector output directly and preserves the
two-hit NanoTrack-verified search recovery contract.

## Acceptance

Host negative fixtures must prove: two consecutive eligible dips bridge, the
third searches, no mature history does not bridge, and geometry/probation/reset
states do not bridge. RK3588 must then pass all 19 CTests, verifier, and the full
271-frame evaluator with exactly one reacquisition. Only a first-run full PASS is
repeated three times.
