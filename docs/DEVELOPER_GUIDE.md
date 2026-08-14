# audio_scripter — Developer Guide

**Status:** experimental research prototype · implementation baseline 0.0.13  
**Updated:** 2026-08-14

This guide documents the current implementation at a level useful for contributors. The language is under active reconsideration; do not infer future language semantics from implementation details described here.

The extended pre-redesign implementation/tutorial guide remains available for provenance on branch `archive/pre-language-redesign-2026-08-14`.

## 1. Architecture

The repository currently contains three layers that are being made conceptually independent:

1. **Script language / parser** — tokenisation, parsing, AST/program representation and evaluation.
2. **Audio runtime** — per-sample execution, persistent state, DSP builtins, program publication, sample-rate/reset handling.
3. **JUCE host** — `AudioProcessor`, VST3/AU/Standalone targets, editor, host parameters, state persistence.

The current code predates this strict separation, so host assumptions still leak into the language (`inL`, `inR`, `outL`, `outR`, `p1`…`p8`). Removing those conceptual leaks is research/design work, not a mechanical refactor.

## 2. Repository layout

```text
Source/
  PluginProcessor.{h,cpp}      JUCE AudioProcessor and host integration
  PluginEditor.{h,cpp}         current code editor and plugin UI
  ScriptTokenizer.{h,cpp}      language tokeniser
  ScriptParser.{h,cpp}         parser, AST/program structures, evaluator
  ScriptEngine.{h,cpp}         runtime entry points and DSP builtins
  ScriptCodeTokeniser.{h,cpp}  editor syntax colouring
  Constants.h                  shared implementation constants
examples/                      .ascr baseline scripts
tests/                         C++ parser/runtime tests
tests_python/                  Python-side tests where applicable
tools/                         validation/report utilities
docs/                          public documentation and project site
```

## 3. Current execution model

The implemented 0.0.13 language runs once for every audio sample. At each sample the runtime provides current inputs and runtime values, evaluates the compiled program, and writes the resulting output samples.

Current special values include:

- `inL`, `inR` — left/right input samples;
- `outL`, `outR` — left/right output samples;
- `sr` — sample rate;
- `t` — elapsed time;
- `p1`…`p8` — normalised host macro parameters.

Ordinary local values are per-sample scratch values. Identifiers beginning `state_` are persistent across samples. Stateful builtins currently use integer lane IDs for independent instances.

The exact baseline language is documented in [`LANGUAGE_SPEC.md`](LANGUAGE_SPEC.md).

## 4. Program compilation and publication

Parsing/validation occurs away from the sample-evaluation path. A successfully constructed program is published as an immutable program snapshot and subsequently read by audio processing. Failed compiles do not replace the previously installed program.

The implementation uses atomic operations around `shared_ptr` program snapshots. **Do not describe this as proven lock-free.** The C++ atomic `shared_ptr` operations used here do not, by themselves, establish a portable lock-free guarantee.

The intended engineering property is that script parsing/compilation and error handling do not occur inside per-sample evaluation. Stronger real-time-safety claims require the allocation/locking audit described below.

## 5. Known real-time-safety gaps

The current baseline is **not yet proven allocation-free or lock-free on the audio path**.

Known issues:

- first use of a `delay()` lane can allocate/initialise its buffer on the audio thread;
- fallback paths for non-pre-resolved stateful builtin instances can construct/hash string keys during processing;
- there is no automated allocation guard covering every shipped example;
- plugin/standard-library operations used by the runtime have not yet been exhaustively audited for blocking behaviour.

Required credibility work:

1. census stateful resources at compile time;
2. allocate delay/state resources before audio processing;
3. remove dynamic string-key fallback from the hot path;
4. add a test that detects heap allocation during representative `processBlock` execution;
5. audit synchronisation paths before making stronger real-time claims.

## 6. Time precision

The current elapsed-time path derives `t` using floating-point seconds. The project has identified precision degradation in long-running sessions. This must be corrected before `t` is treated as a robust long-session oscillator/timebase primitive.

A future language design may also distinguish sample time, physical time, host musical time and tempo-relative time rather than exposing a single scalar `t`.

## 7. DSP builtins

Current builtins include:

- mathematics: `sin`, `cos`, `tan`, `abs`, `sqrt`, `exp`, `log`, `tanh`, `pow`, `min`, `max`;
- shaping: `clamp`, `clip`, `mix`, `wrap`, `fold`, `crush`, `smoothstep`;
- noise/oscillation helpers: `noise`, `pulse`;
- stateful DSP: `lpf1`, `hp1`, `bp1`, `svf`, `slew`, `env`, `delay`;
- saturation: `sat`;
- comparison/selection helpers retained alongside operators.

Stateful builtins currently expose manually numbered lanes. This is a known language-design problem: the lane mechanism is an implementation resource identity, not necessarily a concept the eventual language should expose to a musician.

## 8. Delay implementation

Each current delay lane uses a fixed maximum buffer size of 96,000 samples and fractional reads with interpolation. The current first-use allocation behaviour is a known real-time-safety issue.

Do not build new language semantics around the 96,000-sample implementation constant. Future history/delay semantics must be derived from the language-design research and compiled onto an appropriate runtime resource model.

## 9. Parser and evaluator

The baseline parser supports:

- assignment;
- arithmetic/comparison/logical/bitwise expressions;
- `if` / `else`;
- `while`;
- legacy and extended `for` forms;
- `break` / `continue`;
- top-level user-defined functions and `return`.

The current implementation is a tree-walking evaluator with optimisation work for variable/state lookup and builtin state resolution. Performance is adequate for the existing example corpus but is not itself the research contribution.

Do not add bytecode/JIT infrastructure until it is required by the research prototype or measured runtime limits. Language semantics should not be chosen to simplify the current evaluator.

## 10. Tests

Current validation includes parser/runtime behavioural tests and example-script checks. Run:

```bash
python3 tools/validate_scripts.py
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target audio_scripter_parser_tests
ctest --test-dir build --output-on-failure
```

Important missing coverage:

- deterministic golden-audio numerical regression across the example corpus;
- audio-thread allocation guard;
- long-session timebase regression;
- broader host/plugin-format validation;
- tests for future semantic-kernel prototypes independent of JUCE UI behaviour.

## 11. Building

Standard release helper:

```bash
./scripts/build_release.sh --config Release --package
```

With tests:

```bash
./scripts/build_release.sh --config Release --tests
```

With a local JUCE checkout:

```bash
./scripts/build_release.sh --juce-path /path/to/JUCE --config Release --package
```

See `CMakeLists.txt` and build scripts for the current authoritative build configuration rather than duplicating dependency/version assumptions here.

## 12. Adding code during the language-research phase

### Appropriate now

- correctness fixes;
- test/measurement infrastructure;
- real-time-safety repairs that do not constrain semantics;
- clean separation between language/runtime/host;
- isolated semantic prototypes used to answer an explicit research question;
- benchmark/reproducibility tooling.

### Require design justification first

- arrays/buffers as user-visible language constructs;
- new state syntax;
- parameter declaration syntax;
- musical units;
- history/feedback syntax;
- channel abstractions;
- MIDI/event syntax;
- new control-flow forms;
- standard-library effect abstractions.

A feature being useful or conventional is not sufficient. During this phase, user-visible language changes should identify the representational problem they solve and their strongest prior-art counterexample.

## 13. Public/private research boundary

This public repository should contain:

- implementation facts;
- source/tests/examples;
- conservative, verified project context;
- reproducibility material once reviewed.

Unpublished novelty arguments, detailed competitive comparisons, paper drafts, internal research logs, artwork concepts and funding strategy belong in the private `krahd/academic-writing` workspace.

## 14. Contribution workflow

Before a substantial language change:

1. check [`STATUS.md`](../STATUS.md) and [`ROADMAP.md`](ROADMAP.md);
2. identify the benchmark/research problem motivating the change;
3. check the closest precedent systems;
4. keep the host adapter from becoming the semantic specification;
5. add tests for the intended semantic behaviour;
6. update the language manual only when the baseline implementation actually changes;
7. update `krahd/tom-work-admin` when project state, research direction, dependencies or major gates change, as required by `WORK-ADMIN.md`.

## 15. Provenance

The complete developer guide and implementation state before the 14 August 2026 research reset remain available on:

`archive/pre-language-redesign-2026-08-14`

That branch is intentionally preserved as historical evidence and should not receive new development commits.