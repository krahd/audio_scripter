# audio_scripter — Roadmap to v0.1 ("Musician-led authoring")

Status: draft · target line: **v0.1.0** · supersedes ad-hoc notes in `STATUS.md`

This roadmap turns audio_scripter from a working 0.0.x scriptable plugin into a
**v0.1 release that can carry a peer-reviewed paper**. The paper's contribution
is *coding as an expressive musical gesture*: the musician authoring and
hot-swapping their own audio effects, in-flow, inside the DAW they already play
in (see [RESEARCH.md](RESEARCH.md)). Every item below is justified by whether it
serves that thesis — **low floor** (learnability), **expressive ceiling**
(artistic range), and **systems credibility** (the gesture must be safe and
instantaneous to be musical).

Scope is deliberately **focused** (not a full overhaul). Items marked
*[deferred → v0.2]* are intentionally out of scope for the first paper-ready
release.

---

## 1. Audit summary (2026-06, against `main` @ 0.0.12)

### What is solid
- **Lock-free hot-swap** is correct: programs published as atomic
  `shared_ptr` snapshots; reset / sample-rate as atomic requests consumed on the
  audio thread (`ScriptEngine::compileAndInstall` / `processBlock`,
  `Source/ScriptEngine.cpp`). This is the technical enabler of the whole thesis.
- **Slot-resolution optimisations** are real and well-reasoned: `VarRef`
  (kind + slot), pre-resolved builtin state lanes, per-depth arg-frame pool
  (`Source/ScriptParser.h`, `Source/ScriptParser.cpp`).
- Build / CI / release scaffolding (CMake + FetchContent JUCE 8.0.8, GitHub
  Actions) is appropriate for the project size.

### Findings, by severity

**Critical — contradicts a stated claim**
- **C1. `delay()` allocates on the audio thread.** First use of any lane runs
  `(*ctx.delayBuffers)[lane]` (map insert) and `buf.assign(96000, 0.0f)`
  (~384 KB) inside the audio callback (`Source/ScriptEngine.cpp` `delay`
  builtin). The non-literal-lane fallback in every stateful builtin also builds
  `juce::String` keys and hashes them per sample (`readLaneState` /
  `writeLaneState`). The "real-time safe" claim must be made true and *proven*.
- **C2. `t` precision decays.** `ctx.t = sampleCounter / sr` as `float` loses
  sub-sample precision after ~6 min; time-based oscillators alias in long
  sessions.
- **C3. Parser tests do not run.** `audio_scripter_parser_tests` crashes with
  `std::bad_alloc` before output (per `STATUS.md`). The 733-line test file is
  effectively dead — there is no passing automated correctness gate.

**Language ceiling (expressiveness)**
- **L1. No arrays / tables / buffers.** Everything is a scalar float. Wavetables,
  FIR, granular, convolution, multi-tap structures are impossible without abusing
  hidden `delay` lanes. This is the biggest expressive limit and the weakest
  point versus Faust / Gen~ / Cmajor.
- **L2. Manual integer "lane" state is fragile.** `reverb.ascr` hand-assigns
  lanes 0–13. Error-prone and unteachable — the worst ergonomic wart for a
  *musician-author*.
- **L3. Fixed 8 macro params, all normalised 0..1, no names/ranges/units in the
  language.** Every script re-derives `0.05 + p1*0.45`. No first-class parameter
  declaration.
- **L4. No MIDI / no instrument notion.** `processBlock` does
  `ignoreUnused(midi)`. *[deferred → v0.2]* — relevant to the live-coding-
  instrument angle, not to v0.1's effect-authoring thesis.

**Interface (authoring experience + evaluation surface)**
- **I1. No visual/audio feedback** beyond a single peak number
  (`PluginEditor::timerCallback`). No scope, spectrum, or in/out comparison.
- **I2. Errors go to a text log, not the editor gutter.** No inline markers,
  autocomplete, or inline builtin docs.
- **I3.** Examples are read-only embedded; no patch browser; knobs show no
  ranges/units.

**Performance**
- **P1. AST tree-walking** with `std::function` builtins and per-node virtual
  dispatch. Adequate today (all 24 examples ≥ 1.5× realtime). Bytecode VM is the
  next step and a measurable systems result *[deferred → v0.2]*.

**Docs / process**
- **D1. Version drift:** README & `LANGUAGE_SPEC.md` say `0.0.9`;
  `Constants.h` / CMake say `0.0.12`; CHANGELOG tops out at `0.0.9`.
- **D2. `STATUS.md` is corrupted** — two concatenated snapshots with duplicate
  "Last updated" footers.
- **D3. Validation is regex-based** (`tools/validate_scripts.py`): it lints text,
  it does not verify DSP behaviour. No golden-audio / numerical tests; no
  allocation-on-audio-thread guard.
- **D4. No related-work or evaluation material** — addressed by the rewritten
  `RESEARCH.md`.

---

## 2. v0.1 plan (phased)

### Phase 0 — Hygiene (prerequisite) — ✅ mostly done (2026-06-20)
Goal: a clean, single-source-of-truth baseline that CI actually gates.
- [x] **Single-source the version.** `CMakeLists.txt` `project(VERSION)` is
  canonical and passed via `-DAUDIO_SCRIPTER_VERSION_STRING`; `Constants.h` is a
  documented fallback; README, `LANGUAGE_SPEC.md`, and `docs/index.html` synced
  to 0.0.13. (D1)
- [x] **Repair `STATUS.md`** into one current snapshot; backfill
  `docs/CHANGELOG.md` for 0.0.10–0.0.13. (D2)
- [x] **Investigate the `audio_scripter_parser_tests` crash.** Not a code defect:
  the `std::bad_alloc` came from a **stale `build/` dir bound to the old source
  path** (wrong compiled-in `EXAMPLES_DIR`). A clean configure builds and passes
  `ctest` (1/1). (C3)
- [x] **CI gates the tests.** `.github/workflows/ci.yml` already configures a
  fresh `build/` on a clean runner, builds `audio_scripter_parser_tests`, and
  runs `ctest` on every push/PR — so the stale-path failure cannot recur in CI.
- [ ] Tag a clean baseline (`v0.0.13`) before feature work (user/release action).

### Phase 1 — Credibility floor: RT-safety + correctness
Goal: make the central design claim true and *provable*. This is also Table-1
material for the systems half of the paper.
- [ ] **Parse-time lane/buffer census.** During `compileAndInstall`, enumerate
  every delay lane and stateful-builtin instance and **pre-allocate all buffers
  off the audio thread**. The audio path must do zero allocation and zero string
  hashing. Remove (or assert-unreachable) the `juce::String`-keyed fallback. (C1, L2 groundwork)
- [ ] **Allocation guard test.** Harness that overrides global `new`/`delete` and
  asserts zero heap activity across `processBlock` for every example. (D3)
- [ ] **Golden-audio numerical tests.** Render each example to a fixed buffer with
  a deterministic input; compare against committed references within tolerance.
  Run in CI. (D3)
- [ ] **Fix `t` precision** — phase-accumulate or use `double` for time. (C2)

### Phase 2 — Authoring experience (the thesis core)
Goal: lower the floor and raise the ceiling for a *musician* writing an effect
live. This is what the paper studies and demonstrates.

Language (`Source/ScriptParser.*`, `Source/ScriptEngine.*`, `LANGUAGE_SPEC.md`):
- [ ] **Named state** — `state.foo` (or keep `state_` as alias) replacing raw
  prefixes, and **auto-managed filter/delay instances** so a musician never hand-
  numbers a lane. Backed by the Phase-1 census. (L2)
- [ ] **`param` declarations** — `param drive (0..10, "Drive", default 3)` that
  bind to automatable host parameters, show name/range/unit on the knob, and lift
  the hard 8-param cap to a configurable N. Keep `# @pN` / `# pN =` as legacy. (L3)
- [ ] **Arrays / buffers** — fixed-size, bounds-checked (`buf[i]`), enabling
  wavetables and short FIRs. Allocated at compile time (Phase-1 discipline). (L1)

Interface (`Source/PluginEditor.*`):
- [ ] **Inline error markers** in the editor gutter with messages on the offending
  line (parser already reports line numbers). (I2)
- [ ] **Real-time scope + in/out compare** panel — minimal but present; the core
  immediate-feedback surface for the "gesture" and for study screenshots. (I1)
- [ ] *[stretch]* inline builtin help on hover / autocomplete. (I2)
- [ ] *[stretch]* editable patch browser. (I3)

### Phase 3 — Paper artifacts
- [ ] Finalise `RESEARCH.md` positioning + evaluation design (musician study +
  system benchmarks). (D4)
- [ ] Reproducibility: a CI job / script that regenerates every figure and the
  golden-audio set from a clean checkout.
- [ ] Expand the example library with 2–3 patches that *require* arrays (wavetable
  synth-fx, short convolution/FIR) to demonstrate the new ceiling.

### Deferred to v0.2 (explicitly out of v0.1 scope)
- MIDI / note input / sample-accurate control (live-coding-instrument angle). (L4)
- Bytecode VM + SIMD/block processing + Faust/Cmajor throughput comparison. (P1)
- Multichannel beyond L/R.

---

## 3. Definition of done for v0.1
1. Audio path is allocation-free and lock-free, verified by an automated guard.
2. `ctest` is green in CI and gates merges (parser + golden-audio + allocation).
3. A musician can declare named parameters, use named state, and index a buffer —
   no manual lane integers anywhere in the shipped examples.
4. The editor shows inline errors and live signal feedback.
5. `RESEARCH.md` states the contribution, related work, and a concrete evaluation
   protocol; figures are reproducible from a clean checkout.

## 4. Sequencing notes
- Phases 0–1 are unconditional and unblock everything; do them first.
- Phase-2 language work depends on the Phase-1 census (it is what makes named
  state and arrays allocation-safe).
- Keep each phase shippable: tag `v0.1.0-alpha.N` at phase boundaries so there is
  always a citable, runnable artifact.
