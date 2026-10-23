# RK3566 Debug Runtime Configuration Design

## Intent

Move the interactive Debug executable's flat startup parameters into a
human-editable configuration file so a board operator can start it with
`nanotrack_v3 --config FILE`. Keep temporary command-line overrides for video
and frame-specific experiments.

## Goals

- Support every existing `nanotrack_v3` runtime option in a strict `key=value`
  file, including optional ROI, output and detector options.
- Resolve paths written in the file relative to the file's directory.
- Apply explicit command-line options after the file so they override it.
- Preserve existing command lines and validation rules.
- Ship an interactive board Debug configuration that does not save output by
  default and still permits mouse initialization and re-selection.

## Non-goals and safety constraints

- Do not merge runtime settings into the controller threshold configuration.
- Do not add YAML or another third-party parser dependency.
- Do not add automatic configuration discovery or silently accept unknown,
  duplicate or malformed keys.
- Do not alter tracking, reacquisition, model inference or control decisions.
  In particular, a detector box remains a provisional hypothesis and cannot
  directly set `control_valid=true`.
- Do not commit models, videos, logs or build products.

## Selected approach

Add `--config FILE` as an optional source of the same values accepted by the
CLI. The file uses underscore names such as `model_dir` and
`detector_class_id`; `interactive` is an exact `true` or `false`. Paths from
the file are made absolute relative to the configuration directory before CLI
overrides are applied. Existing CLI path values retain their current working
directory semantics. Validation runs once on the merged result.

The repository's board Debug configuration uses relative repository paths,
starts paused in interactive mode, omits `bbox`, and omits all output paths.
The detector triplet may be enabled only as a complete set and remains subject
to the existing NanoTrack verification and confirmation policy.

## Approaches considered

- **Selected: separate runtime `key=value` file.** Reuses the project's simple
  configuration style, has no extra board dependency, and keeps runtime paths
  separate from algorithm thresholds.
- **YAML runtime file.** Better for nested profiles, but the current values are
  flat and YAML would add parser and deployment complexity without a present
  need.
- **Extend `controller_config`.** Fewer files, but it couples paths and user
  interface behavior to tracking-state thresholds and gives the file two
  unrelated responsibilities.

## Acceptance

- Config-only parsing, relative paths, CLI precedence, strict malformed-input
  rejection and all-or-none detector settings are covered by host tests.
- Legacy CLI tests remain green.
- The integrated Debug source builds and its full RKNN-independent suite passes
  on the host.
- The committed source is transferred to the RK3566 repository, clean-built in
  its native environment, and its board tests pass. An interactive launch must
  load the config and reach the GUI without enabling output persistence by
  default; operator-visible tracking quality is not inferred from this startup
  check.
