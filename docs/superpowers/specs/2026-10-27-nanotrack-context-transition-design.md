# NanoTrack Factorial Journal Context Transition Design

## Intent

Resume the interrupted Task 6 screening journal after the reviewed restart-
determinism fix, without weakening source identity checks or rewriting the
provenance of attempts completed under the original controller.

The transition exists only because the journal correctly binds controller and
scoring-policy source hashes. The fix changes those two hashes while leaving the
training data, model, pair policy, configurations, evaluators, commands, and
sample schedule unchanged.

## Goals

- Require an explicit, separately retained transition manifest before a new
  controller context may consume the historical schema-2 journal.
- Keep the journal's original `context` and every historical attempt/candidate
  identity unchanged.
- Register the new full context as a chained, audited transition and bind all new
  attempts to that context.
- Validate each completed attempt with the exact context recorded in its own
  identity.
- Invalidate, retain, and retry nonterminal attempts created under an older
  context; never continue their partial training under new source code.
- Preserve strict artifact, command, path, checkpoint, configuration, pair-policy,
  evaluator, and data identities.

## Non-goals

- This mechanism does not authorize arbitrary source upgrades, schema migration,
  model promotion, hidden-set access, or repair of damaged artifacts.
- It does not restore the falsely invalidated candidate record. That node is
  rerun in a new attempt while the old completed and interrupted attempts remain
  historical evidence.
- It does not alter scoring thresholds, training semantics, matrix shape, or
  selection rules.

## Considered Approaches

### Rewrite all historical identities to the new context

This would make the existing validator simple, but would falsely imply that old
training and evaluation ran under new source. Rejected.

### Canonicalize the journal and resume from the old commit

This could avoid a source transition, but would require changing persisted
decisions and continuing with code that still contains the restart bug. It would
also split the branch identity from the running controller. Rejected.

### Preserve per-attempt contexts with an explicit transition manifest

Selected. The original context remains immutable, the new context is appended,
and validation resolves each attempt by `context_sha256`. This costs additional
context-registry validation but gives accurate provenance and safe future
restarts.

## Transition Manifest

The opt-in JSON uses schema version 1 and contains:

- `old_context_sha256` and `new_context_sha256`;
- `reason` as a non-empty string;
- `allowed_code_changes`, an exact mapping from source identity key to
  `{old_sha256, new_sha256}`.

For this recovery, the mapping must contain exactly:

- `code:run_nanotrack_factorial.py`;
- `code:nanotrack_factorial_policy.py`.

The final real manifest is generated only after implementation and review, when
the final controller SHA and resulting context SHA are known. Its absolute path,
SHA-256, size, and content are stored in the journal transition record. A later
resume rechecks the manifest artifact before accepting its registered context.

## State and Validation Model

- `state.context` remains the original full context.
- Optional `state.context_transitions` is an ordered chain. Each entry records the
  old/new context hashes, the complete new context, the retained transition-
  manifest identity, and the exact allowed code changes.
- Every context recomputes its own hash. Each transition must start at the prior
  chain tail, and no duplicate or fork is accepted.
- The current runtime context must equal the chain tail. If it differs and no
  transition manifest is supplied, the controller fails before writing state or
  executing commands.
- A supplied transition is accepted only when the full old/current context diff
  consists of the two declared source SHA fields plus the derived top-level
  context SHA. Paths and all other fields must match exactly.
- Candidate and attempt validation resolves the exact context named by the
  record's `identity.context_sha256`. The baseline has no node identity and
  remains bound to the original context.

## Recovery Behavior

After the transition record is atomically persisted and before any external
command runs:

- completed old-context attempts remain reusable and are never relabeled;
- the protection-stopped `b32_g0/123456/2560/attempt0002` is invalidated with an
  explicit context-transition reason and a new attempt is allocated;
- the power-loss-interrupted `b64_g30/20261011/2560/attempt0001` is handled the
  same way when traversal reaches it;
- the already false-invalidated attempt0001 and its candidate record remain in
  their existing audit collections;
- all newly allocated attempts use the new context and scoring-source hashes.

## Safety and Evidence Boundaries

Transition validation and persistence must happen while holding the controller
lock. Rejection leaves the journal byte-identical and runs no external command.
Acceptance never rewrites historical contexts, attempts, candidates, invalidated
records, checkpoint files, logs, or evaluations. Hidden inputs and `sot-v1.mp4`
remain prohibited. This is desktop experiment recovery only and provides no
ONNX, RKNN, RK3566, or gimbal-pod evidence.

