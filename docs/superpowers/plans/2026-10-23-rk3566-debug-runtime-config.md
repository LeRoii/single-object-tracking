# RK3566 Debug Runtime Configuration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Start the RK3566 interactive Debug tracker with `nanotrack_v3 --config FILE` while preserving strict validation and legacy CLI compatibility.

**Architecture:** Integrate the existing interactive Debug commits onto the current coordination baseline. Add a focused flat runtime-config loader consumed by `parse_options`; it normalizes config-owned paths, merges explicit CLI values last, and reuses the existing option validators. Keep controller thresholds and runtime startup settings in separate files.

**Tech Stack:** C++17, `std::filesystem`, CMake/CTest, OpenCV HighGUI, native RK3566 GCC/CMake environment.

**Spec:** `docs/superpowers/specs/2026-10-23-rk3566-debug-runtime-config-design.md`

## Global Constraints

- Use strict `key=value`; do not introduce YAML or another dependency.
- Every current `nanotrack_v3` option must be representable in the runtime file.
- Config-relative paths are resolved against the config directory; CLI paths keep current working-directory semantics.
- CLI values override config values; legacy CLI-only commands remain valid.
- Interactive Debug output remains disabled unless an output path is explicit.
- Detector files remain an all-or-none triplet and detector boxes never directly authorize control.
- Do not commit models, videos, logs or build artifacts.
- Only update `docs/progress/board-validation.md` under `docs/progress/`.

## Review Focus

- A config path containing `..`, spaces or an absolute path resolves without depending on the launch directory; Task 2 tests it.
- An explicit CLI video or frame value overrides the corresponding config value without being treated as a duplicate; Task 2 tests it.
- Unknown keys, duplicate keys, malformed lines and invalid booleans fail before model startup; Task 2 tests them.
- A partial detector file triplet supplied across config and CLI either becomes complete after merge or fails existing validation; Task 2 tests both branches.
- `interactive=false` still enforces headless bbox/output requirements, while `interactive=true` may omit them; Task 2 tests both branches.

---

### Task 1: Integrate the interactive Debug baseline

**Files:**
- Merge source branch: `codex/rk3566-interactive-debug@b98b1335c3d263f542fd52656c7d7c4d58b2bb77`
- Verify: `rk3566/nanotrack_v3/`

**Interfaces:**
- Consumes: current coordination baseline `main@bb297504070b3dfcf2fd4b4da05ebb1503e309db`
- Produces: one branch containing coordination documents and the interactive player implementation.

- [ ] **Step 1: Merge the committed interactive branch without rewriting either history.**
- [ ] **Step 2: Configure with `BUILD_TESTING=ON` and `ENABLE_RKNN_RUNTIME=OFF`.**
- [ ] **Step 3: Build and run CTest; expect all 13 integrated baseline tests to pass.**

### Task 2: Parse and merge the runtime configuration

**Files:**
- Create: `rk3566/nanotrack_v3/include/runtime_config.hpp`
- Create: `rk3566/nanotrack_v3/src/runtime_config.cpp`
- Modify: `rk3566/nanotrack_v3/src/cli.cpp`
- Modify: `rk3566/nanotrack_v3/include/cli.hpp`
- Modify: `rk3566/nanotrack_v3/CMakeLists.txt`
- Test: `rk3566/nanotrack_v3/tests/test_cli.cpp`

**Interfaces:**
- Produces: `RuntimeConfig load_runtime_config(const std::string& path)` with strict unique keys and config-relative path normalization.
- Consumes: `parse_options(const std::vector<std::string>&)` merges loaded values with later explicit CLI values and returns the existing `Options` type.

- [ ] **Step 1: Add config-only, relative-path, CLI-override, boolean, invalid syntax/key, detector merge and headless validation tests.**
- [ ] **Step 2: Build and run `test_cli`; confirm RED failures are caused by missing `--config` support.**
- [ ] **Step 3: Implement the minimal runtime loader, `--config` extraction and merged validation.**
- [ ] **Step 4: Rebuild and run `test_cli`; expect PASS.**
- [ ] **Step 5: Run the complete host CTest suite; expect zero failures.**

### Task 3: Add the operator configuration and documentation

**Files:**
- Create: `rk3566/nanotrack_v3/config/interactive-debug.conf`
- Modify: `rk3566/nanotrack_v3/README.md`
- Test: `rk3566/nanotrack_v3/tests/test_cli.cpp`

**Interfaces:**
- Produces: a board-oriented, no-output-by-default config and the exact one-argument launch command.

- [ ] **Step 1: Add a test that loads the shipped configuration from an arbitrary working directory and verifies interactive/no-output behavior.**
- [ ] **Step 2: Run `test_cli`; confirm RED while the shipped file is absent.**
- [ ] **Step 3: Add the configuration and document keys, precedence, path rules and launch usage.**
- [ ] **Step 4: Rebuild and run full host CTest; expect zero failures.**

### Task 4: Commit, deploy and validate on RK3566

**Files:**
- Modify after observed validation: `docs/progress/board-validation.md`
- Board source: `/home/cat/Project/SOT/nanotrack_v3_longterm`
- Board build: `/home/cat/Project/SOT/nanotrack_v3_longterm/build-interactive-debug-config`

**Interfaces:**
- Consumes: the committed shared-source feature revision and unchanged model/release artifacts.
- Produces: a board-native executable identity, exact validation commands/results and an updated board-validation handoff boundary.

- [ ] **Step 1: Commit source/tests/docs, then transfer only that committed change to the independent board repository.**
- [ ] **Step 2: Confirm board time is corrected, preserve unrelated files, and clean-configure/build natively.**
- [ ] **Step 3: Run board CTest and a bounded `--config` startup/smoke check; record observed output and artifact hashes.**
- [ ] **Step 4: Update only `docs/progress/board-validation.md` with timestamp, commits, paths, hashes, commands, results, boundaries and next action.**
- [ ] **Step 5: Commit the progress update and report both commit IDs to the coordinating conversation.**
