# Repository coordination instructions

This repository is developed by several Codex chats working on NanoTrack
fine-tuning, detector training, RK3566 development-board validation, and
production cross-compilation. Chat transcripts are not a shared source of
truth. Durable coordination lives under `docs/progress/` and in Git history.

## Before starting work

1. Read `docs/progress/STATUS.md`.
2. Read the progress file for the task you own.
3. Read `docs/progress/integration-contract.yaml` and every upstream task file
   named by your task.
4. Run `git status --short`, `git branch --show-current`, and
   `git rev-parse HEAD`. Report unexpected local changes or a baseline mismatch
   before editing overlapping files.
5. Treat live source, manifests, artifact hashes, and observed test output as
   authoritative. Plans and previous chats are not completion evidence.

## Ownership

- NanoTrack fine-tuning chat owns `docs/progress/nanotrack-finetune.md`.
- Detector training chat owns `docs/progress/detector-training.md`.
- Production cross-compilation chat owns `docs/progress/cross-compile.md`.
- Development-board validation chat owns `docs/progress/board-validation.md`.
- The coordinating chat alone owns `docs/progress/STATUS.md` and
  `docs/progress/integration-contract.yaml`.

Task chats must not directly rewrite another task's progress file. Record a
requested downstream change in the task's `Handoff` section and give the
coordinating chat the commit hash.

## Updating progress

After a material milestone, update the owned task file in the same commit as
the code or documentation that produced it. Record:

- an ISO 8601 timestamp with timezone;
- branch, worktree, base commit, and resulting commit when available;
- exact input and output paths plus SHA-256 for models and binaries;
- commands actually run and concise observed results;
- blockers, evidence boundaries, downstream effects, and next action.

Do not mark work complete merely because files exist or a build succeeded.
Distinguish desktop inference, ONNX export, RKNN conversion, board model load,
board inference, and end-to-end board acceptance. Large/generated artifacts
remain outside Git; commit only their manifests, provenance, hashes, and
reproduction commands.

## Git and worktrees

Use one branch/worktree per code-changing task. A task becomes visible to
other worktrees only after its commit is integrated into their baseline.
Before consuming an upstream result, verify the exact commit and artifact
hashes against `integration-contract.yaml`. The cross-compilation chat must
compile only the latest coordinator-approved development-board handoff. Never
copy uncommitted changes between worktrees as a synchronization mechanism.

Preserve unrelated or pre-existing changes. Do not delete, reset, overwrite,
or relabel historical evidence. If the user requests a read-only check, do not
update progress files.

## Development-board and production boundary

Only an observed run on the stated RK3566 board is board evidence. Every board
claim must bind the source commit, executable hash, model/manifest/label
hashes, board runtime/driver versions, input identity, command, measurement
boundary, and retained output paths. Detector candidates never directly
authorize control; only NanoTrack-verified reacquisition may set
`control_valid=true`.

The development board has a complete native build environment. Native build,
interactive debugging, and board testing happen before production
cross-compilation. After the board-validation chat hands off the approved
latest source and artifact identities, the cross-compilation chat only needs
to confirm that it can obtain that exact result and compile the production
artifact. Deployment to and testing on the real gimbal pod are human-only
steps. Never claim pod acceptance without an explicit human report.
