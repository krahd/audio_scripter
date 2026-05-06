# audio_scripter — Project Status

Last updated: 2026-05-06 08:07

## Current focus

Investigating and fixing audio glitches ("destroyed + echo") that users hear when running heavier effects in a real-time DAW (Ableton Live 12, 48 kHz). Offline rendering is correct; the problem is **realtime CPU performance** of the script engine.

## Root cause (confirmed)

The script engine is **slower than realtime** for many of the example effects, which causes Ableton buffer underruns. Underruns sound like glitches that the user perceives as "destroyed" audio with comb-filter / echo artifacts.

Confirming evidence:
- Muting the audio_scripter track but leaving the plugin instantiated still distorts other tracks (because Ableton runs muted plugins; CPU starvation affects the whole graph).
- Offline export of the same track sounds clean (offline does not have realtime deadlines).
- Benchmark of `ScriptEngine::processBlock` over 6 s of audio at 48 kHz, 256-sample blocks:
  - autopan, ms_width, simple_delay, chorus → ≥ 1.3× realtime → work in Ableton
  - autowah, exp_compressor, cos_phaser, formant_robot, reverb, svf_morph_sweeper → < 0.6× realtime → fail in Ableton

## Hot paths in the engine (profiled)

1. `EvalContext::locals.clear()` runs every sample. Currently a `std::unordered_map<juce::String,float>` — its destructor frees per-entry nodes.
2. `EvalContext::getValue / setValue` walk an `if (name == ...)` chain of up to 16 string compares before a map lookup.
3. `FunctionCallExpr::evaluate` allocates a fresh `std::vector<float>` on every call for arguments.
4. `FunctionCallExpr::evaluate` previously called `functionName.toLowerCase()` per call (now cached at parse time).
5. AST is interpreted via virtual dispatch; every operator is one or two `evaluate()` calls.

## Changes already on `main`

| Commit | What |
|---|---|
| `6988b8b` | Smoothed env attacks + slew-smoothed parameter modulation in autowah / exp_compressor / cos_phaser / formant_robot to reduce transient artifacts; ms_width tanh output clamp; reverb all-pass state-tracking fix; output peak-level meter in editor |
| (uncommitted, pending verification) | `std::map` → `std::unordered_map<juce::String, float, JuceStringHash>` for `locals`, `persistentState`, and the function registry; `FunctionCallExpr` caches `functionNameLower` at parse time so the hot path no longer allocates a new juce::String per call |

The "uncommitted" optimizations gave a ~30–40 % speedup but most heavy effects are still below 1.0× realtime, so glitches in Ableton persist.

## Plugin version

Currently `0.0.10` (CMakeLists.txt + Source/Constants.h). Both VST3 and AU components are at the same version after running `cmake --build build --target audio_scripter_All`.

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
| `~/temp/test.wav`, `~/temp/test2.wav` | User-provided Ableton bounce: dry vs autowah |

## Next steps (priority order)

### 1. Make the engine ≥ 2× realtime for the heavy effects

The minimal-disruption optimization is exhausted. The next steps are bigger:

- **Resolve variable names to slot indices at parse time** (the proper fix). `VariableExpr` and `AssignmentStatement` would store a `(VarKind, int slot)` pair instead of `juce::String name`. `EvalContext` would hold `std::vector<float>` for locals and a pointer to `std::vector<float>` for state, indexed directly. `clear()` becomes `std::fill(... 0.0f)` over a small fixed array. Targets: 5–10× speedup on hot paths.
- **Avoid `std::vector<float>` allocation per builtin call**. Either change `BuiltinFunction` to take a `(const float*, size_t)` (touches every builtin), or pool/reuse a thread-local args buffer.
- **Optional follow-up**: compile the AST to a flat bytecode tape so `processBlock` is a tight `switch` over opcodes rather than virtual-call AST walking.

### 2. Rebuild + reinstall + verify in Ableton

After each optimization step:
- `cmake --build build --target audio_scripter_All --config Release` (builds VST3 and AU; auto-installs).
- Bump `project(...)` version in `CMakeLists.txt` and `AUDIO_SCRIPTER_VERSION_STRING` in `Source/Constants.h` so you can confirm the right build is loaded (verify in the plugin's About link).
- Restart Ableton fully — it caches plugin code in-process.
- Re-run `./build/audio_scripter_render_report /tmp/render_<name> examples` and check `realtime=` for each script before testing in the DAW.

### 3. Documentation / housekeeping

- The previous "transient artifacts" hypothesis (smoothed envs, slew on cutoffs) was correct but only addressed offline glitches. Keep those changes — they make the effects sound better even once the realtime perf is fixed.
- The level meter in the title bar shows post-effect peak; useful for confirming audio is flowing without a DAW meter handy.
- Consider raising `kMaxInstructionsPerSample` (currently 4096) only after the per-instruction cost drops; raising it now would just make complex scripts even slower.

## Known oddities (probably not bugs)

- The render report's synthetic "program" input has step transients at every beat (every 0.5 s). This produces 2 Hz "click" artifacts in offline renders of envelope-driven effects — those are real responses to a discontinuous test signal, not an engine bug.
- `runtimeState.sampleCounter` is a `uint64_t` cast to `float` for `ctx.t`. After ~6 minutes of continuous playback (sampleCounter > 2^24 at 48 kHz) `t` loses sub-2-sample precision; high-frequency oscillators using `t` (e.g., the formant_robot carrier) will alias slightly. Not the current problem, but worth fixing if we ever care about long-running session quality.
