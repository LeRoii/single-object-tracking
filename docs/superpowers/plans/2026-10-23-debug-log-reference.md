# RK3566 Debug Log Reference Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add an implementation-accurate Chinese reference for every structured interactive Debug event and field.

**Architecture:** Keep the reference in the existing end-to-end code-flow document and organize it by event family. Bind every field to its producing source structure or local runtime value, while separating the standalone Debug executable from the production `ai_alg` ABI.

**Tech Stack:** Markdown, C++17 source inspection, shell consistency checks

**Spec:** `docs/superpowers/specs/2026-10-23-debug-log-reference-design.md`

## Global Constraints

- Document only events emitted when the standalone program runs with `interactive=true`.
- Do not claim that ViewLink `ai_alg` emits these events.
- Detector candidates never directly authorize control.
- Do not change runtime code, thresholds, state transitions, or the public integration contract.

## Review Focus

- Every literal `DEBUG_EVENT event=` name in both emitters must appear in the reference.
- `frame` must distinguish source-video numbering from `ControllerOutput::frame_id` without implying they are different in the current loop.
- `accepted`, `reacquired`, and `control_valid` must not be described as synonyms.
- Missing optional candidate ID/region must be documented as `none`, not zero.
- Heartbeat cadence must remain one forced event per manual step and one periodic event per 30 processed tracking frames.

---

### Task 1: Document the structured Debug log contract

**Files:**
- Modify: `RK3566_NANOTRACK_LONGTERM_CODE_FLOW.zh-CN.md`
- Modify: `docs/progress/board-validation.md`

**Interfaces:**
- Consumes: event emitters in `rk3566/nanotrack_v3/src/main.cpp` and `rk3566/nanotrack_v3/src/debug_telemetry.cpp`
- Produces: a searchable Chinese event/field reference and a board-validation documentation handoff record

- [ ] **Step 1: Extract the complete event and field inventory**

Record lifecycle events from `main.cpp`, frame-result events from `debug_telemetry.cpp`, and field sources from the runtime/controller headers.

- [ ] **Step 2: Add the Debug log reference chapter**

Describe scope, syntax, cadence, lifecycle events, state/detector/template/heartbeat events, enum values, field sources, safety interpretation, and a practical reading sequence.

- [ ] **Step 3: Record the documentation milestone**

Append an ISO-8601 entry to `docs/progress/board-validation.md` with branch, base, changed document, checks, evidence boundary, and coordinator handoff.

- [ ] **Step 4: Verify source-to-document consistency**

Run event-name extraction against both C++ emitters and assert that all emitted names appear in the new chapter; run `git diff --check`.

- [ ] **Step 5: Commit**

Stage only the design, plan, code-flow document, and owned board-validation progress file; inspect `git diff --cached --name-status`; commit the documentation milestone.
