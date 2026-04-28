# Effects engine and example-behavior analysis

Date: 2026-04-28

## Scope and method

This note reviews the local repository implementation for:

- DSP engine execution model
- built-in effect math/state logic
- parser/runtime safety guards
- all scripts in `examples/`

Validation commands used:

- `python3 tools/validate_scripts.py`
- a local token-similarity script across `examples/*.ascr`
- a local macro-usage scan across `examples/*.ascr`

## Does the engine/effects logic work as intended?

Short answer: **mostly yes** for the currently documented behavior.

### What is implemented correctly

1. **Per-sample execution with deterministic variable reset semantics**
   - Every sample resets `outL/outR` to passthrough (`inL/inR`) before script statements run.
   - `locals` are cleared each sample; `state_` variables persist in `persistentState`.

2. **Built-ins expected by examples are present and wired**
   - math (`sin/cos/tan/abs/sqrt/exp/log/tanh/pow/min/max`)
   - shaping (`clamp/clip/mix/wrap/fold/crush/smoothstep/noise`)
   - logic (`gt/lt/ge/le/select`)
   - DSP/stateful helpers (`pulse/lpf1/slew/env`)

3. **Stateful helpers are lane-separated**
   - `lpf1`, `slew`, and `env` use internal keys (`__function_lane`) so left/right or custom lanes can maintain independent memory.

4. **Runtime guardrails exist**
   - instruction budget per sample (`4096`)
   - loop depth guard
   - recursion depth guard
   - statement count cap at parse time

### Important caveats (can change perceived behavior)

1. **No strict arity/type enforcement at parse/compile time**
   - Built-ins use a helper that substitutes missing args with `0.0`.
   - So malformed calls can become silently different behavior instead of compile errors.

2. **Persistent-state cap can evict by map order**
   - cap is `128` entries; eviction removes `begin()` key (lexicographic-first), not LRU.
   - Typical scripts are fine, but heavy dynamic state-key usage can lose old values in non-obvious order.

3. **If EXAMPLES_DIR fallback path is used, built-in example name/script indexing is inconsistent**
   - `exampleNames()` and `exampleScript()` are offset/misaligned (soft clip appears under cross-feedback slot in fallback list).
   - In normal builds with `EXAMPLES_DIR`, the filesystem examples are used and this mismatch is bypassed.

## Why many examples can sound very similar

This repo behavior strongly supports your observation.

### 1) Macro defaults make many examples nearly dry by default

Plugin parameter defaults are:

- `p1 = 0.25`
- `p2..p8 = 0.0`

Many examples use `p3` or `p2` as **depth/wet/resonance** controls. With defaults at zero, several scripts reduce to subtle or almost dry output until macros are moved.

### 2) Several examples are intentionally from the same effect family

The set includes multiple related non-linear processors and modulations:

- saturation/distortion/wavefold variants
- gating/tremolo/pulse variants
- filtered modulation variants

So even when active, many produce comparable "color" families rather than maximally orthogonal sounds.

### 3) Some examples are intentionally minimal templates

A subset are compact demonstrations (few lines, no user functions), often designed to show one concept. Those can sound closer to each other on broadband program material than the richer examples.

### 4) Stereo symmetry and shared modulators reduce perceived contrast

Many scripts process L/R symmetrically or with small parameter offsets, so the stereo image and macro-dynamics can remain similar between presets.

## Practical checks to distinguish examples audibly

1. Start every new example by setting:
   - `p3` (if present) to `0.7–1.0` for wet-heavy scripts
   - `p2` above `0` where used for depth/resonance
2. Use a static source (sustained synth + drums) and level-match.
3. Compare by family:
   - `ring_mod_fixed` vs `phase_fm`
   - `highpass_filter` vs `cos_phaser`
   - `sample_hold` vs `sample_rate_reducer`
4. Avoid judging at factory defaults only.

## Confidence conclusion

- **Engine core:** robust enough for intended per-sample scripted effects.
- **Effect implementations:** mathematically coherent and state handling is consistent with docs/examples.
- **Your “they sound similar” observation:** likely real and mostly explained by default macro values + overlapping example design families, not by one obvious catastrophic DSP bug.
