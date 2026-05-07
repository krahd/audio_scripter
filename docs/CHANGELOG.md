# Changelog

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
