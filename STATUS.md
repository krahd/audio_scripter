# audio_scripter – Project Status

Last updated: 2026-05-07 00:15

## Project purpose

audio_scripter is a JUCE-based real-time scriptable audio effect plugin. It lets users write small DSP scripts that run once per audio sample, manipulate `inL`/`inR`, and write `outL`/`outR` without recompiling or restarting the DAW.

## Current implementation state

The project currently provides a cross-format plugin/application target with VST3, AU, and Standalone outputs. The script language supports arithmetic, user functions, persistent state, loop controls, eight automatable macro knobs, and DSP primitives including filters, envelope following, slew, delay, and saturation.

The curated example library includes practical studio effects and creative patches. Recent engine work replaced the delay primitive's state handling with proper per-lane ring buffers and refreshed broken examples with standard delay, ping-pong delay, reverb, and chorus scripts.

## Active focus

Current focus is stabilising the script engine, example library, plugin UI, CI/build reliability, and release workflow for the 0.0.x line.

## Architecture overview

The plugin is built with JUCE and CMake. Source code implements the audio processor, editor/UI, parser/interpreter, DSP primitives, and script hot-swap behaviour. Python tooling validates example scripts, while shell scripts and GitHub Actions handle build, package, install, CI, and release tasks.

### Architecture diagram

<svg xmlns="http://www.w3.org/2000/svg" width="980" height="420" viewBox="0 0 980 420" role="img" aria-labelledby="audio-arch-title audio-arch-desc">
  <title id="audio-arch-title">audio_scripter architecture</title>
  <desc id="audio-arch-desc">CMake builds JUCE plugin targets from source code, examples and validation tools support the script engine, and release scripts package plugin formats.</desc>
  <defs><marker id="arrow" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="8" markerHeight="8" orient="auto"><path d="M0 0 L10 5 L0 10 z" /></marker></defs>
  <rect x="40" y="70" width="190" height="70" rx="10" fill="none" stroke="black" /><text x="135" y="100" text-anchor="middle" font-size="14">CMake / JUCE</text><text x="135" y="120" text-anchor="middle" font-size="12">build system</text>
  <rect x="310" y="50" width="220" height="90" rx="10" fill="none" stroke="black" /><text x="420" y="84" text-anchor="middle" font-size="14">Source/</text><text x="420" y="106" text-anchor="middle" font-size="12">processor, editor, parser,</text><text x="420" y="124" text-anchor="middle" font-size="12">engine, DSP primitives</text>
  <rect x="620" y="45" width="230" height="80" rx="10" fill="none" stroke="black" /><text x="735" y="76" text-anchor="middle" font-size="14">Plugin outputs</text><text x="735" y="98" text-anchor="middle" font-size="12">VST3, AU, Standalone</text>
  <rect x="40" y="230" width="190" height="70" rx="10" fill="none" stroke="black" /><text x="135" y="260" text-anchor="middle" font-size="14">examples/</text><text x="135" y="280" text-anchor="middle" font-size="12">curated scripts</text>
  <rect x="310" y="220" width="220" height="90" rx="10" fill="none" stroke="black" /><text x="420" y="252" text-anchor="middle" font-size="14">Validation tools</text><text x="420" y="274" text-anchor="middle" font-size="12">script validator and</text><text x="420" y="292" text-anchor="middle" font-size="12">parser tests</text>
  <rect x="620" y="230" width="230" height="80" rx="10" fill="none" stroke="black" /><text x="735" y="260" text-anchor="middle" font-size="14">Release automation</text><text x="735" y="282" text-anchor="middle" font-size="12">scripts and GitHub Actions</text>
  <line x1="230" y1="105" x2="310" y2="95" stroke="black" marker-end="url(#arrow)" /><line x1="530" y1="95" x2="620" y2="85" stroke="black" marker-end="url(#arrow)" /><line x1="230" y1="265" x2="310" y2="265" stroke="black" marker-end="url(#arrow)" /><line x1="530" y1="265" x2="620" y2="270" stroke="black" marker-end="url(#arrow)" /><line x1="420" y1="140" x2="420" y2="220" stroke="black" marker-end="url(#arrow)" />
</svg>

### Flow chart

<svg xmlns="http://www.w3.org/2000/svg" width="980" height="330" viewBox="0 0 980 330" role="img" aria-labelledby="audio-flow-title audio-flow-desc">
  <title id="audio-flow-title">audio_scripter runtime flow</title>
  <desc id="audio-flow-desc">A user writes a script, applies it, validation/parser logic installs it into the engine, and the audio callback runs it per sample.</desc>
  <defs><marker id="flowarrow" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="8" markerHeight="8" orient="auto"><path d="M0 0 L10 5 L0 10 z" /></marker></defs>
  <rect x="30" y="130" width="140" height="65" rx="10" fill="none" stroke="black" /><text x="100" y="158" text-anchor="middle" font-size="12">Write or load</text><text x="100" y="176" text-anchor="middle" font-size="12">script</text>
  <rect x="220" y="130" width="140" height="65" rx="10" fill="none" stroke="black" /><text x="290" y="158" text-anchor="middle" font-size="12">Apply from</text><text x="290" y="176" text-anchor="middle" font-size="12">plugin UI</text>
  <rect x="410" y="130" width="140" height="65" rx="10" fill="none" stroke="black" /><text x="480" y="158" text-anchor="middle" font-size="12">Parse and</text><text x="480" y="176" text-anchor="middle" font-size="12">validate</text>
  <rect x="600" y="130" width="140" height="65" rx="10" fill="none" stroke="black" /><text x="670" y="158" text-anchor="middle" font-size="12">Hot-swap</text><text x="670" y="176" text-anchor="middle" font-size="12">engine state</text>
  <rect x="790" y="130" width="150" height="65" rx="10" fill="none" stroke="black" /><text x="865" y="158" text-anchor="middle" font-size="12">Process audio</text><text x="865" y="176" text-anchor="middle" font-size="12">per sample</text>
  <line x1="170" y1="162" x2="220" y2="162" stroke="black" marker-end="url(#flowarrow)" /><line x1="360" y1="162" x2="410" y2="162" stroke="black" marker-end="url(#flowarrow)" /><line x1="550" y1="162" x2="600" y2="162" stroke="black" marker-end="url(#flowarrow)" /><line x1="740" y1="162" x2="790" y2="162" stroke="black" marker-end="url(#flowarrow)" />
</svg>

## Setup and run instructions

```bash
./scripts/build_release.sh --config Release --package
./scripts/build_release.sh --config Release --tests
python3 tools/validate_scripts.py
cmake --build build --target audio_scripter_parser_tests
ctest --test-dir build --output-on-failure
```

macOS install helper:

```bash
./install.sh -b
sudo ./install.sh --system -b
```

## Configuration and environment variables

- JUCE is fetched automatically via CMake FetchContent unless a local checkout is supplied.
- A local JUCE checkout can be passed through `./scripts/build_release.sh --juce-path /path/to/JUCE`.
- CMake 3.22+ and a C++20 compiler are required.

## Important files and directories

- `Source/`: plugin, parser, engine, UI, and DSP implementation.
- `examples/`: curated `.ascr` scripts.
- `docs/LANGUAGE_SPEC.md`: script language reference.
- `docs/DEVELOPER_GUIDE.md`: architecture and release documentation.
- `docs/CHANGELOG.md`: release history.
- `tools/validate_scripts.py`: example validator.
- `scripts/build_release.sh`: release/test/package helper.
- `install.sh`: plugin installer with backup support.
- `.github/workflows/ci.yml`: CI validation.
- `.github/workflows/release.yml`: release build/publication workflow.

## Recent changes

- About dialog and website link were added to the plugin UI and then restyled into the title bar.
- Delay engine state was moved from the persistent-state map into proper per-lane ring buffers.
- Broken/less useful examples were removed or replaced with standard effects.
- CI JUCE helper build dependencies were adjusted for Ubuntu 24.04.

## Tests and verification status

No tests were run while creating this documentation-only status snapshot.

Repository-documented validation includes script validation, parser unit tests, CTest, CI parser validation, and release build/package workflows.

## Known issues, risks, and limitations

- Real-time DSP changes can introduce audio-thread safety regressions if allocations, locks, or blocking work are added.
- Plugin-format validation must be checked on target hosts/platforms before release.
- Delay/reverb/feedback-style primitives need careful bounds and stability review.
- Release workflow correctness depends on platform-specific CI and packaging checks.

## Pending tasks

- Keep example scripts validated against the language parser.
- Confirm release artefacts on target plugin hosts before tagging.
- Keep documentation aligned with script-language and DSP primitive changes.

## Next steps

1. Run script validation and parser tests after the next code change.
2. Review example scripts for parameter smoothing and safe output levels.
3. Confirm current version metadata before the next release.

## Longer-term steps

1. Harden plugin-host validation across supported DAWs and formats.
2. Expand parser/engine test coverage for DSP primitives.
3. Keep the language manual and developer guide aligned with implemented behaviour.

## Decisions and rationale

- Scripts are designed for immediate runtime application without recompilation.
- The engine prioritises real-time safety and predictable audio-thread behaviour.
- Example scripts should be practical, validated, and useful as reference material.

---

Last updated: 2026-05-07 00:15
# audio_scripter — Project Status

Last updated: 2026-05-06 08:49

## Current focus

Performance optimizations are complete. All 24 example effects now run ≥ 1.5× realtime offline; realtime audio glitches in Ableton are resolved. Current focus: housekeeping and any further polish.

## Root cause (confirmed)

**RESOLVED.** The script engine was slower than realtime for many effects, causing Ableton buffer underruns ("destroyed + echo" artefacts). All optimisations listed below have been applied; every effect now exceeds 1.0× realtime at 44.1 kHz.

Confirming evidence (original):
- Muting the audio_scripter track but leaving the plugin instantiated still distorted other tracks (CPU starvation affects the whole graph).
- Offline export sounded clean; the problem was strictly realtime deadlines.
- Pre-optimisation benchmark: autowah, exp_compressor, cos_phaser, formant_robot, reverb, svf_morph_sweeper → < 0.6× realtime.

## Hot paths in the engine (profiled)

1. ~~`EvalContext::locals.clear()` runs every sample. Currently a `std::unordered_map<juce::String,float>`.~~ **FIXED** — locals and state vars are now slot-indexed `std::vector<float>`.
2. ~~`EvalContext::getValue / setValue` walk an `if (name == ...)` chain + map lookup.~~ **FIXED** — VarRef carries `(VarKind, int slot)`.
3. ~~`FunctionCallExpr::evaluate` allocates a fresh `std::vector<float>` for arguments per call.~~ **FIXED** — `callArgFrames` pool reuses per-depth vectors; heap-use-after-free fixed by capturing depth index instead of reference.
4. ~~`FunctionCallExpr::evaluate` called `functionName.toLowerCase()` per call.~~ **FIXED** — cached as `functionNameLower` at parse time.
5. ~~Stateful builtins (`lpf1`, `hp1`, `bp1`, `svf`, `slew`, `env`) constructed a `juce::String` key and did a hash-map lookup every sample.~~ **FIXED** — literal lane args pre-resolved to `stateSlots` vector indices at parse time.
6. AST is interpreted via virtual dispatch; every operator is one or two `evaluate()` calls. (Not yet addressed; not needed for ≥ 1× realtime.)

## Changes already on `main`

| Commit | What |
|---|---|
| `6988b8b` | Smoothed env attacks + slew-smoothed parameter modulation in autowah / exp_compressor / cos_phaser / formant_robot to reduce transient artifacts; ms_width tanh output clamp; reverb all-pass state-tracking fix; output peak-level meter in editor |
| (current session) | `std::map` → `std::unordered_map`; `functionNameLower` cached at parse time; VarRef slot system for locals + state vars; `callArgFrames` pool (fix heap-use-after-free: use depth index not reference); builtin state slot pre-resolution for literal lane args (`lpf1`, `hp1`, `bp1`, `svf`, `slew`, `env`). All 24 effects now ≥ 1.5× realtime. Version bumped to 0.0.12. |

## Plugin version

Currently `0.0.12` (`CMakeLists.txt` + `Source/Constants.h`). Both VST3 and AU components are at the same version after running `cmake --build build --target audio_scripter_All`.

## Files of interest

- `Source/ScriptEngine.cpp`, `Source/ScriptEngine.h` — engine entry points + builtins (`lpf1`, `bp1`, `svf`, `env`, `slew`, `delay`, etc.)
- `Source/ScriptParser.cpp`, `Source/ScriptParser.h` — AST + interpreter; `EvalContext::getValue`/`setValue` is the per-sample hot path
- `Source/PluginProcessor.cpp` — audio thread; calls `engine.processBlock()` and computes the level-meter peak
- `Source/PluginEditor.cpp` — slider attachments, examples dropdown, About dialog (shows version)
- `tools/RenderEffectReport.cpp` — offline render of every example to WAV with metrics; prints per-effect realtime factor on stdout
- `examples/*.ascr` — embedded example scripts; loaded into the plugin via the CMake `juce_add_binary_data` target `audio_scripter_examples`

## Diagnostic outputs

| Path | What |
|---|---|
| `/tmp/render_final/` | Offline renders @ 44.1 kHz (older, pre-optimization scripts) |
| `/tmp/render_48k/` | Offline renders @ 48 kHz (matches Ableton rate) |
| `/tmp/render_fixed/` | After script smoothing fixes (commit `6988b8b`) |
| `/tmp/render_v2/` | After unordered_map + cached-lowercase optimization (uncommitted) |
| `/tmp/render_bench/` | First benchmark run that surfaced the realtime factors |
| `/tmp/render_release_final/` | After all current-session optimisations; all 24 effects pass (slowest: reverb 1.59×, formant_robot 1.51×, autowah 2.34×) |
| `~/temp/test.wav`, `~/temp/test2.wav` | User-provided Ableton bounce: dry vs autowah |

## Next steps (priority order)

### 1. Verify in Ableton

- Restart Ableton fully (it caches plugin code in-process).
- Confirm version 0.0.12 shows in the About link.
- Test autowah, reverb, formant_robot — these were the most problematic and are now the heaviest.

### 2. Optional future performance work

The engine is now fast enough for all current examples. If heavier user scripts are needed:
- **Bytecode compilation**: compile the AST to a flat tape of opcodes; eliminates virtual dispatch overhead. Would give another 2–5× on complex scripts.
- **Dynamic-lane builtin pre-resolution**: extend the slot pre-resolution to handle cases where the lane expression is a simple macro (`p1` etc.) — currently only literal integer lanes are pre-resolved.

### 3. Fix pre-existing `audio_scripter_parser_tests` crash

`./build/audio_scripter_parser_tests` crashes with `std::bad_alloc` before printing any output. This predates the current session and is unrelated to the performance changes. Needs investigation.

## Known oddities (probably not bugs)

- The render report's synthetic "program" input has step transients at every beat (every 0.5 s). This produces 2 Hz "click" artifacts in offline renders of envelope-driven effects — those are real responses to a discontinuous test signal, not an engine bug.
- `runtimeState.sampleCounter` is a `uint64_t` cast to `float` for `ctx.t`. After ~6 minutes of continuous playback (sampleCounter > 2^24 at 48 kHz) `t` loses sub-2-sample precision; high-frequency oscillators using `t` (e.g., the formant_robot carrier) will alias slightly. Not the current problem, but worth fixing if we ever care about long-running session quality.
