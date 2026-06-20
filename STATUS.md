# audio_scripter — Project Status

Last updated: 2026-06-20 · version 0.0.13

## Project purpose

audio_scripter is a JUCE-based, real-time scriptable audio effect plugin (VST3,
AU, Standalone). Users write small per-sample DSP scripts that read `inL`/`inR`,
compute, and write `outL`/`outR`, and hot-swap them with zero audio interruption.

The next milestone (v0.1) is framed around a research thesis: **coding an audio
effect, in-flow, inside the DAW, as an expressive musical gesture**. See
[docs/ROADMAP.md](docs/ROADMAP.md) for the audit + plan and
[docs/RESEARCH.md](docs/RESEARCH.md) for paper positioning.

## Current implementation state

Cross-format plugin/application target (VST3, AU, Standalone). The script
language supports arithmetic, comparison/logical/bitwise operators, user
functions, persistent `state_` variables, loop controls (`break`/`continue`),
eight automatable macro knobs `p1..p8`, and DSP builtins (`lpf1`, `hp1`, `bp1`,
`svf`, `env`, `slew`, `delay`, `sat`, plus math/shaping/noise helpers). 24
curated example effects ship embedded in the binary.

Performance work (0.0.10–0.0.12) made all 24 examples run ≥ 1.5× realtime
offline and resolved realtime glitches: slot-indexed locals/state, cached
lowercased function names, a per-depth call-arg pool, and pre-resolved literal
builtin state lanes.

## Active focus — v0.1, Phase 0 (housekeeping)

Phase 0 establishes a clean, citable baseline before feature work.

- **Done:** synced all version strings to `CMakeLists.txt` (0.0.13); documented
  CMake as the canonical version source; added `docs/ROADMAP.md`; rewrote
  `docs/RESEARCH.md`; rewrote this status file; backfilled `docs/CHANGELOG.md`
  for 0.0.10–0.0.13.
- **Resolved:** the reported `audio_scripter_parser_tests` `std::bad_alloc` was a
  **stale build directory** bound to the old source path
  (`.../ml-llm/llm/audio_scripter`), not a code defect. A clean CMake configure
  builds and passes `ctest` (1/1 passed).
- **Remaining for Phase 0:** tag the `v0.0.13` baseline.

## Build, test, run

```bash
# Configure + build tests (fresh build dir; reuse a JUCE checkout if available)
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target audio_scripter_parser_tests
ctest --test-dir build --output-on-failure

# Validate example scripts (text-level lint)
python3 tools/validate_scripts.py

# Release build + package (VST3 / AU / Standalone)
./scripts/build_release.sh --config Release --package
```

> Note: if `cmake --build build` errors with "CMakeCache.txt directory is
> different than the directory where CMakeCache.txt was created", the `build/`
> dir was configured for a different absolute path — delete it and reconfigure.

macOS install helper:

```bash
./install.sh -b
sudo ./install.sh --system -b
```

## Architecture overview

JUCE + CMake (JUCE 8.0.8 via FetchContent, or a local checkout via
`--juce-path`). `Source/` holds the audio processor, editor/UI, parser/
interpreter, and DSP builtins. Scripts compile off the audio thread and publish
as immutable `shared_ptr<const CompiledProgram>` snapshots loaded atomically by
the audio thread. Reset / sample-rate changes are atomic requests consumed at
block boundaries.

## Important files

- `Source/ScriptEngine.{h,cpp}` — engine entry points and DSP builtins.
- `Source/ScriptParser.{h,cpp}` — tokenizer-fed parser, AST, evaluator; the
  per-sample hot path (`EvalContext::getValue`/`setValue`).
- `Source/PluginProcessor.cpp` — audio thread; calls `engine.processBlock`.
- `Source/PluginEditor.cpp` — editor, examples dropdown, peak meter, About box.
- `examples/*.ascr` — embedded example scripts.
- `docs/ROADMAP.md`, `docs/RESEARCH.md`, `docs/LANGUAGE_SPEC.md`,
  `docs/DEVELOPER_GUIDE.md`, `docs/CHANGELOG.md`.
- `tools/validate_scripts.py`, `tools/RenderEffectReport.cpp`.

## Tests and verification status

- `audio_scripter_parser_tests` builds clean and `ctest` passes (1/1) as of
  2026-06-20.
- Coverage is parse-level + behavioural smoke tests (gain, silence, tanh, delay,
  lpf1, abort fallback, function scope, reset, per-example NaN/Inf +
  loudness/diversity heuristics). There are **no golden-audio numerical tests**
  and **no audio-thread allocation guard** yet — both are Phase 1.

## Known issues, risks, limitations (carried into v0.1)

- **RT-safety gap:** `delay()` allocates on the audio thread on first use of a
  lane (map insert + ~384 KB buffer); the non-literal-lane builtin fallback
  hashes `juce::String` keys per sample. Fixing this (parse-time lane census +
  pre-allocation) is Phase 1.
- **`t` precision** decays after ~6 minutes (`float` seconds from a sample
  counter); time-based oscillators alias in long sessions. Phase 1.
- Manual integer "lane" management for stateful builtins is fragile (see
  `reverb.ascr`). Phase 2 (named state / auto-managed instances).
- Plugin-format validation must be confirmed on target hosts before release.

## Next steps

1. Tag `v0.0.13` as the Phase 0 baseline.
2. Phase 1: parse-time lane census → allocation-free audio path; allocation
   guard + golden-audio tests; fix `t` precision.
3. Phase 2: named state, `param` declarations, arrays/buffers; inline editor
   errors + live scope.

See [docs/ROADMAP.md](docs/ROADMAP.md) for the full phased plan and definition of
done.
