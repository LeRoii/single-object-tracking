# NanoTrack Factorial Journal Context Transition Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add an explicit, audit-preserving context transition that lets the reviewed controller safely resume the interrupted schema-2 screening journal without rewriting historical provenance.

**Architecture:** Keep the original context immutable and append a strictly chained registry of approved contexts. Validate each historical attempt against the context hash stored in its identity, invalidate nonterminal old-context attempts, and bind only newly allocated attempts to the active context.

**Tech Stack:** Python 3.7, JSON, SHA-256, `unittest`, existing atomic journal and fake-controller fixtures.

**Spec:** `docs/superpowers/specs/2026-10-27-nanotrack-context-transition-design.md`

## Global Constraints

- Work only on `codex/nanotrack-factorial-v3` in the existing isolated worktree.
- Preserve `state.context` and all historical attempt/candidate/invalidated identities and artifacts.
- Accept a transition only through an explicit schema-1 manifest whose full diff is exactly the controller and scoring-policy SHA changes.
- Keep all model, data, evaluator, configuration, pair-policy, command, path, and artifact checks exact.
- Reject before journal mutation or command execution when the manifest is absent, malformed, stale, broadened, or its retained artifact changes.
- Never continue a nonterminal attempt across contexts; retain/invalidate it and allocate a new attempt.
- Do not access hidden evaluation content or run `sot-v1.mp4`.
- Keep generated manifests, journals, logs, and models outside Git. Modify only NanoTrack code/tests/docs and the owned progress file.
- Follow RED -> GREEN and independently review the implementation before real recovery.

## Review Focus

- A transition with an extra changed code/data field must fail without changing journal bytes or invoking the runner.
- A transition whose manifest is modified after adoption must fail on the next resume.
- Completed attempts spanning old and new contexts must validate with their own recorded contexts.
- Old-context `planned` or `running` attempts must never execute under the new context.
- A second transition, duplicate transition, forked chain, or unknown context hash must fail closed.

---

### Task 1: Implement and Verify the Context Registry and Opt-in Transition

**Files:**

- Modify: `scripts/run_nanotrack_factorial.py`
- Modify: `tests/test_nanotrack_factorial_controller.py`
- Modify: `docs/progress/nanotrack-finetune.md`

**Interfaces:**

- Consumes: schema-2 state, the runtime context returned by `load_context`, and an optional transition-manifest path.
- Produces: validated `context_transitions`, per-identity context resolution, and `--context-transition PATH`.

- [ ] **Step 1: Write failing behavioral tests.** Cover no-manifest rejection with byte-identical journal/no runner calls; malformed hashes or any extra context difference; successful append without modifying original context or historical records; changed retained manifest; duplicate/fork/unknown contexts; mixed-context completed validation; and invalidation/retry of old-context nonterminal attempts.
- [ ] **Step 2: Run focused tests against unchanged production.** Expected: failures caused by the missing transition interface and the current exact single-context assumption, with zero test-code errors.
- [ ] **Step 3: Implement the minimal registry and manifest validator.** Add helpers to recompute/resolve contexts and verify an exact chained transition. Require only the two source keys named by the design; store the complete new context and manifest artifact identity.
- [ ] **Step 4: Integrate the transition under the controller lock.** Add `context_transition=None` to `run_screening` and `--context-transition` to the CLI. Persist an accepted transition atomically before reconciliation or any external command. Leave rejection byte-identical.
- [ ] **Step 5: Make validation context-aware.** Validate baseline with the original context, each node/attempt with its identity context, and all retained transition manifests on every resume. Preserve exact artifact and command checks.
- [ ] **Step 6: Prevent cross-context continuation.** Before `run_attempt`, invalidate any nonterminal attempt whose identity context is not the active context, retain its audit inventory, and allocate a unique current-context attempt.
- [ ] **Step 7: Run focused, relevant historical, and complete unittest discovery.** Report known unrelated failures exactly; do not claim the full suite is green if they remain.
- [ ] **Step 8: Update the owned progress file and commit code, tests, and implementation evidence together.** The reviewed design/plan commit is the task input; do not rewrite it during implementation. Do not create the real transition manifest or resume training until independent review approves the final source identities.
