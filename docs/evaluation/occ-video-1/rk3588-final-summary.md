# RK3588 current result

## C010 post-holdout candidate

C010 is a partial failure, not an accepted final candidate. Board Debug/RKNN build,
CTest 19/19, schema verifier, full-occlusion motion-control semantics, identity proxy,
trajectory continuity, and visible-box quality all pass. Frame 200 uses the new bounded
`template_bridge`, but the anchor peak-margin dip lasts through frame 201. Because C010
permits only one bridge frame, search starts at 201 and a second reacquisition occurs at
217. Full evaluator reacquisition frames are `[185, 217]`; therefore the full verdict is
`FAIL` and no repeat runs were performed.

Retained board evidence:
`/home/neardi/Project/SOT/occ-candidate-033dc90/runs/C010-full-run1`.

## C009 frozen holdout result

- Result: **FAIL / rejected for release**.
- Frozen runtime source: `05cf4d92debd6799be2b59102b6546078f30a8e8`.
- Freeze record commit: `4800be9432f45bc95271e54d970bcb2dbb4f437c`.
- Board evidence: `/home/neardi/Project/SOT/occ-candidate-05cf4d9/runs/C009-full-run1`.
- Frozen command executions: one. Repeat runs stopped after the first holdout failure.

The requested first occlusion behavior remains correct: the tracker keeps
`MOTION_SEARCH` control through the full occlusion and reacquires at frame 185.
The formal holdout nevertheless fails because it enters another search while the
target is visible at frames 200--215, reacquires again at frame 216, and achieves
only `0.671046` valid-observation mean IoU against the frozen `0.70` floor.

Identity proxy, continuous control, and prediction continuity pass. The artifact
verifier separately rejects frame 204 because a present `CONFLICT` hint has
confirmation count zero. The holdout evaluator also has a scope-applicability bug
for the full-occlusion gate, but removing that false gate cannot turn this candidate
into a pass.

No full-scope evaluator, repeat run, parameter retuning, or human identity sign-off
was performed after the failure. The holdout is now exposed; future work must be
reported as post-holdout development rather than another blind attempt.

## Retained identities

- bboxes: `97f275ae1bf05eb345bb8335757ffbcaac063153d895c1f3ccd0808792f4c645`
- output video: `ac527664f99b1211d6ffa505d4bb65f9cc69b7795b7aaf870c97d3de30a63f36`
- session metadata: `f0ea84dd87bbe59c8566ed854218879efaff46bc7983810a00fafd635fd2a480`
- holdout evaluator JSON: `dae7168e3df422b42796ba156e70f7c1bfb67b0fb65596e63a3791a439343ffe`
- holdout evaluator Markdown: `9782515043a6b14835e6e9fa027e22518daa82a0584c04ffc880e4d46839baf7`
- holdout evaluator log: `f31bc0bc07419d5df004782d6fe9457328d3f0dbe6af930d67168db5f76d2c76`
- RK3588 verifier failure log: `5cfcc80e859666701405b05899443c765d8325ae08d5b842913b47ea8dd2d448`
