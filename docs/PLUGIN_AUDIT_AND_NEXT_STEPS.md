# Plugin audit and next steps (April 27, 2026)

This document audits the current `audio_scripter` codebase and proposes a concrete path to a fully functional, well-designed plugin.

## Executive summary

- The product vision is strong: scriptable per-sample DSP, macro automation, lock-free script swapping, and a usable editor already exist in architecture/docs.
- However, the current parser/runtime integration is inconsistent and likely not build-stable in its present state (duplicated `EvalContext`, partial function-registry migration, and mismatched runtime defaults).
- The shortest path to “fully functional” is **stabilization first**, then **language/runtime completion**, then **UX and release hardening**.

## Current-state assessment

## What is already good

1. **Solid product framing and release scaffolding**
   - README, changelog, roadmap, and language specs are present and versioned.
2. **Plugin shell and host integration are in place**
   - JUCE plugin targets (VST3/AU/Standalone), APVTS parameters, state save/restore, and editor wiring exist.
3. **Script UX baseline exists**
   - Editor, Apply/Save/Load, examples, and help panel are implemented.

## Key gaps blocking “fully functional” status

1. **Core parser/runtime model is internally inconsistent**
   - `ScriptParser.h` defines `EvalContext` twice (one pre-declaration struct and one full runtime struct), which introduces ambiguity and likely compile break risk.
   - Runtime code writes `ctx.functionRegistry`, but that field is not present in the trailing concrete `EvalContext` struct in `ScriptParser.h`.
   - `ScriptParser.cpp` contains AST implementations before including `ScriptParser.h`, with unqualified types (`BlockStatement`, `EvalContext`) outside the namespace, making translation-unit correctness fragile.

2. **Runtime semantics drift from spec/docs**
   - Language spec says defaults are `outL=inL`, `outR=inR`; engine currently initializes outputs to `0.0f`, which can mute passthrough behavior unless script explicitly writes outputs.

3. **Function registry is only partially implemented**
   - Engine clears registry and only explicitly registers `sin`, while docs/help/examples imply broad function support.
   - Function-call path includes TODO/no-op behavior for user-defined functions.

4. **Quality gates are incomplete for realtime DSP confidence**
   - Current script validator is static and useful, but there are no unit tests for tokenizer/parser semantics, no deterministic DSP golden tests, and no CI matrix for plugin build targets.

## Recommended implementation plan

## Phase 0 (Critical: restore correctness and build stability)

1. **Unify `EvalContext` definition into exactly one struct**
   - Include all required fields (`audio IO`, `macros`, `locals`, `persistent state`, `function registry`, and any safety guards).
2. **Normalize AST implementation placement and namespace hygiene**
   - Ensure all AST method definitions are under `namespace scripting` and after declarations are visible.
3. **Make runtime defaults match documented semantics**
   - Initialize outputs to input passthrough before statement execution.
4. **Add a “known-good smoke test” script at startup**
   - Compile default script and assert expected compile success path in debug/test target.

**Definition of done for Phase 0:** clean build, plugin loads in host, scripts compile/apply, and passthrough works when outputs are not explicitly overwritten.

## Phase 1 (Language completeness and deterministic behavior)

1. **Complete built-in function registration from spec**
   - Register all documented built-ins in one table-driven registry.
2. **Choose and finish user-defined function behavior**
   - Either fully implement call stack + return semantics, or temporarily disable `fn` with explicit compile errors.
3. **Harden parser diagnostics**
   - Improve line/column diagnostics for arity/type/control-flow errors.
4. **Cap runtime work deterministically**
   - Keep statement budget and add instruction budget per sample (already aligned with roadmap intent).

**Definition of done for Phase 1:** language behavior matches docs exactly; no advertised feature is partial/silent-no-op.

## Phase 2 (UX and plugin ergonomics)

1. **Editor usability upgrades**
   - Syntax highlighting, inline compile markers, jump-to-error.
2. **Preset/script browser improvements**
   - Taggable examples, search/filter, recently-used list.
3. **Macro ergonomics**
   - Expose meaningful names/tooltip metadata and optional script-side defaults.
4. **Safety UX**
   - Add compile warnings for common pitfalls (unused vars, undefined outputs, dangerous loop patterns).

## Phase 3 (release engineering and trust)

1. **Automated tests**
   - Tokenizer/parser unit tests
   - AST evaluation unit tests
   - DSP golden render tests (offline buffers with deterministic seeds)
2. **CI/CD**
   - Linux/macOS build verification (and Windows if available), validator job, artifact packaging.
3. **Performance telemetry in debug builds**
   - Per-block timing + instruction counters to catch regressions early.
4. **Documentation synchronization policy**
   - Changelog/spec/readme checks in PR template to avoid drift.

## Prioritized next 10 tasks (practical backlog)

1. Remove duplicate `EvalContext` and compile errors in parser headers/sources.
2. Add `functionRegistry` field to canonical context or refactor call path.
3. Refactor AST method definitions into consistent namespace blocks.
4. Change output initialization to passthrough (`outL=inL`, `outR=inR`).
5. Implement all documented built-ins with centralized arity validation.
6. Decide `fn/return` support level and enforce it (full or explicitly unsupported).
7. Add parser unit tests for assignment, precedence, function calls, and blocks.
8. Add runtime tests for persistent `state_`, macros `p1..p8`, and `lpf1/slew/env` state lanes.
9. Add deterministic integration tests for all example scripts in `examples/`.
10. Add CI workflow to run build + tests + `tools/validate_scripts.py`.

## Product-level acceptance criteria for “fully functional and well-designed”

- **Correctness:** language spec and runtime behavior are aligned and test-backed.
- **Realtime safety:** bounded per-sample cost and no unbounded constructs without guardrails.
- **UX:** compile/apply loop is fast, errors are understandable, and examples are trustworthy.
- **Release quality:** cross-platform builds reproducible with CI, docs current, and regression suite green.

