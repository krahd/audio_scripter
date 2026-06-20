# Changelog

## 0.0.13 - 2026-06-20

### Changed
- Project housekeeping baseline for the v0.1 roadmap: synced all version strings
  (README, language manual, website) to the canonical version in `CMakeLists.txt`.
- Documented that `CMakeLists.txt` is the single source of truth for the version;
  `Source/Constants.h` now only provides a non-CMake fallback.

### Docs
- Added `docs/ROADMAP.md` (audit + focused v0.1 plan) and rewrote
  `docs/RESEARCH.md` into a paper-positioning document.
- Rewrote `STATUS.md` into a single current snapshot (was two concatenated
  snapshots).

### Fixed
- `audio_scripter_parser_tests` no longer fails to run: the previously reported
  `std::bad_alloc` was a stale build directory bound to an old source path, not a
  code defect. A clean CMake configure builds and passes `ctest`.

## 0.0.10–0.0.12 - 2026-05

### Performance
- Replaced per-sample `std::unordered_map` locals/state access with slot-indexed
  `std::vector<float>`; `VarRef` now carries `(VarKind, slot)`.
- Cached lowercased function names at parse time; added a per-depth `callArgFrames`
  pool to avoid per-call argument allocation (fixing a heap-use-after-free).
- Pre-resolved literal builtin state lanes (`lpf1`, `hp1`, `bp1`, `svf`, `slew`,
  `env`) to slot indices at parse time.
- Result: all 24 example effects run ≥ 1.5× realtime offline; realtime glitches
  resolved.

### Added
- Output peak-level meter in the editor; About dialog with version and website.

### Changed
- Moved delay state from the persistent-state map into per-lane ring buffers.

## 0.0.9 - 2026-04-30

### Changed
- Embed curated example scripts into the plugin binary so hosts do not depend on source-tree `.ascr` files at runtime.
- Replace redundant or stale demo presets with a clearer standard/creative palette, including `formant_robot` and `ring_modulator`.
- Rework reverb into a damped, diffused plate-style patch to avoid obvious broken comb echoes.
- Interpolate fractional `delay()` reads so modulated chorus, doubler, and delay scripts avoid integer-step zipper artifacts.
- Add render-level example regression checks for wetness, stability, unintended tails, peak level, and near-duplicate output.
- Keep runtime reset/sample-rate changes audio-thread-owned via atomic requests.
- Prune stale internal audit notes, old release zips, and local Ableton backup artifacts from the repository.

### Fixed
- Runaway scripts now fall back to dry passthrough for the aborted sample.
- User-defined function locals are call-scoped while `state_` variables remain shared DSP state.
- Macro default parsing now only reads `# pN = value` metadata comments.

## 0.0.8 - 2026-04-29

### Changed
- Reset project versioning to pre-1.0: `0.0.8` reflects the early-stage API.

### Notes
- Earlier `v1.x` tags were experimental and should not be treated as current release history.
