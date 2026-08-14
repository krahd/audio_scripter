# Agent Instructions — audio_scripter

These instructions apply to AI coding agents working in this repository.

## Repository status

`audio_scripter` is an **experimental research prototype**, currently at implementation baseline `0.0.13`. The current `.ascr` language is a preserved research baseline, not a stable API or the final language design.

The project is now research-first: do not accumulate user-visible language features simply because they are useful or conventional. The semantic design is being reconsidered independently of the VST/AU interface.

The pre-redesign state is preserved on branch:

`archive/pre-language-redesign-2026-08-14`

Do not develop on or rewrite that provenance branch.

## Architecture boundary

Treat these as distinct layers even where the current baseline still couples them:

1. **language** — semantics and notation;
2. **runtime** — evaluation, state, time, memory, DSP resources, safety;
3. **host adapter** — JUCE/VST3/AU/Standalone integration, parameters, transport, persistence;
4. **authoring interface** — editor, diagnostics and feedback.

The plugin is one host for the language, not the language specification. New semantics must not be justified solely by what is convenient for JUCE/VST.

## Repository layout

| Path | Purpose |
|---|---|
| `Source/` | Plugin C++ source, parser, runtime, editor and processor |
| `examples/` | Bundled `.ascr` baseline effect scripts |
| `tests/` | CTest-based C++ parser/runtime tests |
| `tests_python/` | Python-side validation tests where applicable |
| `tools/` | Offline render / benchmark utilities |
| `docs/` | Public language, developer, research and roadmap documentation |
| `scripts/` | Build and packaging scripts |

## Build baseline

`CMakeLists.txt` is authoritative for build requirements. At baseline 0.0.13 it specifies:

- CMake 3.22+;
- C++20;
- JUCE 8.0.8 through FetchContent unless a local JUCE checkout is supplied.

Typical commands:

```bash
# Release build
bash scripts/build_release.sh --config Release

# Debug build + install
bash scripts/build_release.sh --config Debug --install

# Run configured tests
ctest --test-dir build --output-on-failure
```

Build artefacts are generated under `build/`; do not commit them.

## Real-time safety — current truth

The audio path **is not yet proven allocation-free or lock-free**.

Known issues include:

- first use of a `delay()` lane can allocate/initialise its buffer on the audio thread;
- fallback stateful-builtin paths can perform string construction/hash lookup during processing;
- an automated audio-thread allocation guard is not yet present;
- use of atomic `shared_ptr` program publication does not itself establish a portable lock-free guarantee.

Therefore:

- do not describe the current engine as allocation-free, lock-free, or fully real-time-safe;
- avoid introducing any new allocation, blocking, I/O, logging, string construction, container growth, or mutex acquisition into per-sample/audio-callback paths;
- prefer compile-time/off-audio-thread resource census and allocation;
- add tests/measurement before strengthening any real-time-safety claim.

Repairing these issues without prejudging future language semantics is appropriate work.

## Code conventions

- C++20.
- Keep exceptions/RTTI out of hot audio paths unless the existing build/configuration explicitly requires otherwise.
- Avoid allocation or potentially allocating JUCE/STL operations in `ScriptEngine::processBlock` and functions called per sample.
- Use `juce::String` outside audio processing unless a path has been explicitly audited.
- Parser implementation belongs in `ScriptParser.*` / `ScriptTokenizer.*`.
- Runtime/builtin implementation belongs in `ScriptEngine.*`.
- Keep `Source/Constants.h` and CMake project versioning consistent when changing releases.

Do not add a new builtin merely because it is convenient. During the language-research phase, a user-visible semantic addition needs a documented representational problem and prior-art check.

## Language-change rule

Before adding or changing user-visible syntax/semantics:

1. read `STATUS.md`, `docs/ROADMAP.md`, and `docs/RESEARCH.md`;
2. identify the research/benchmark problem the change addresses;
3. check the strongest relevant precedent system rather than assuming the feature is absent elsewhere;
4. distinguish semantic change from editor/runtime/host convenience;
5. implement the smallest prototype necessary to test the hypothesis;
6. add behavioural tests;
7. update `docs/LANGUAGE_SPEC.md` only when the implementation actually changes.

Large feature work such as arrays/buffers, new state syntax, parameters, history/feedback constructs, musical units, channel abstractions, MIDI/event syntax or new control-flow forms is **not pre-authorised by the old roadmap**.

## Testing

After engine/parser changes:

```bash
cmake --build build --target audio_scripter_parser_tests
ctest --test-dir build --output-on-failure
```

After adding/modifying examples, use the repository's current validation tooling, including:

```bash
python3 tools/validate_scripts.py
```

For performance-sensitive work, the offline render/report target is available:

```bash
cmake --build build --target audio_scripter_render_report
./build/audio_scripter_render_report
```

Do not turn a historical benchmark threshold into a correctness claim. Record the exact test configuration and results when performance matters.

## Documentation boundary

This repository is public.

Public material may include:

- verified implementation facts;
- source/tests/examples;
- conservative, source-checked research context;
- reviewed reproducibility material.

Do **not** add unpublished novelty arguments, detailed competitive judgements, paper drafts, internal theoretical notes, unreviewed artwork concepts, funding strategy, or private reflective-practice logs. Those belong in the private `krahd/academic-writing` workspace.

## STATUS.md — mandatory upkeep

`STATUS.md` must remain current after any material change. Record:

- current focus and research/technical gate;
- verified root causes of open technical issues;
- changes already on `main`;
- altered next actions;
- material implementation/research state that affects other repositories.

Use factual wording and distinguish implemented behaviour, measured evidence, and hypotheses.

## Cross-repository administration

`krahd/tom-work-admin` is canonical for global/cross-domain status. Follow `WORK-ADMIN.md` and update `tom-work-admin` in the same work session whenever lifecycle, research direction, major validation state, publication relationship, name/status, or next cross-domain action changes.

Private paper/theory work belongs in:

`krahd/academic-writing/my_papers_2026/2026 - Programmable Audio Language/`

## Pull-request / commit hygiene

- Keep commits focused.
- Use concise imperative commit subjects.
- Do not commit generated build artefacts or `.DS_Store`.
- Do not force-push `main`.
- Do not rewrite the frozen provenance branch.

## Safety rules

- Treat `build/` as generated output.
- Do not run destructive recursive deletion without explicit user instruction/confirmation where required.
- Do not delete provenance/history to make the current narrative cleaner.
- Preserve failed language designs and benchmark evidence when they have research value.