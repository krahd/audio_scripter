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
