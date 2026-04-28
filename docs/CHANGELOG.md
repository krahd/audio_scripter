# Changelog

## 1.1.0 - 2026-04-28

### Added
- New DSP primitives: `hp1`, `bp1`, `svf`, `delay`, and `sat`.
- Extended loop control: `for (i = start; condition; step) { ... }` plus `break;` and `continue;`.
- New examples using the new primitives: `svf_morph_sweeper`, `micro_delay_widen`; plus updated `iter_fold` and `highpass_filter`.
- Validator heuristics that warn about always-on noise and unsmoothed narrow pulse gates.

### Changed
- Bump project version to `1.1.0` and sync docs.

## 1.0.9 - 2026-04-28

### Changed
- Refresh core example presets to reduce static-like artifacts and make effects more distinct across the library.
- Improve rhythm/gate/mod examples with smoother transitions (`slew`) and more musical modulation blends.
- Improve noise/crush examples by reducing always-on full-band noise and adding tonal smoothing/wet staging.
- Bump project version to `1.0.9` and align README release/version references.

## 1.0.8 - 2026-04-28

### Added
- Release packages (VST3, AU, Standalone) attached to GitHub release `v1.0.8`.

### Changed
- Bump project version to `1.0.8`.

## 1.0.6 - 2026-04-27

### Added
- Release packaging and CI-triggered binary artifacts for macOS and Windows.

### Changed
- Bump version to `1.0.6` and update README and About box text.
- Trigger release workflow via pushed annotated tag `v1.0.6`.

## 1.0.5 - 2026-04-24

### Changed
- Bump version to `1.0.5` and update packaging, docs, and minor build fixes.
- Add editor and plugin screenshots to documentation.

## 1.0.4 - 2026-04-23

### Added
- New envelope follower primitive: `env(x, attack, release [, id])`.
- New example script: `examples/envelope_duck_tremor.ascr`.

### Changed
- Built-in example browser now includes "Envelope duck tremor".
- Validator/docs updated for `env` function support.

## 1.0.3 - 2026-04-23

### Added
- New comparator/control functions: `gt`, `lt`, `ge`, `le`, `select`.
- New timing utility: `pulse(freqHz, duty)`.
- New example script: `examples/rhythmic_pulse_gate.ascr`.

### Changed
- Example browser and docs updated for the new control/timing primitives.
- Script validator allowlist expanded for new functions.

## 1.0.2 - 2026-04-23

### Added
- New language functions: `smoothstep(edge0, edge1, x)` and deterministic pseudo-random `noise(seed)`.
- New example script: `examples/noisy_transient_gate.ascr`.

### Changed
- Example browser now includes "Noisy transient gate".
- `tools/validate_scripts.py` updated to validate the new function names.

## 1.0.1 - 2026-04-23

### Added
- 8 macro parameters (`p1..p8`) wired to DAW automation and script runtime.
- Script size guard (256 statements max).
- Empty-script compile validation.
- New examples: `wavefold_shimmer`, `stereo_bit_crush_drift`.
- Language and release documentation updates.

### Changed
- Runtime upgraded to lock-free compiled program swapping (atomic snapshot model).
- Default script now demonstrates macro-driven drive control using `p1`.
- Plugin state serialization now includes both script source and parameter tree state.

### Notes
- Build supports either local JUCE path (`AUDIO_SCRIPTER_JUCE_PATH`) or FetchContent clone.
