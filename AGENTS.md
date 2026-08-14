# Agent Instructions — audio_scripter

These instructions apply to AI coding agents working in this repository.

## Repository status

`audio_scripter` is an **experimental research prototype**, currently at implementation baseline `0.0.13`. The current `.ascr` language is a preserved research baseline, not a stable API or the final language design.

The project is research-first: do not accumulate user-visible language features simply because they are useful or conventional. The semantic design is being reconsidered independently of the VST/AU interface.

The pre-redesign state is preserved on branch:

`archive/pre-language-redesign-2026-08-14`

Do not develop on or rewrite that provenance branch.

The canonical private research workspace is:

`krahd/research/projects/audio_scripter/`

## Architecture boundary

Treat these as distinct layers even where the current baseline still couples them:

1. **language** — semantics and notation;
2. **runtime** — evaluation, state, time, memory, DSP resources, safety;
3. **host adapter** — JUCE/VST3/AU/Standalone integration, parameters, transport, persistence;
4. **authoring interface** — editor, diagnostics and feedback.

The plugin is one host for the language, not the language specification. New semantics must not be justified solely by what is convenient for JUCE/VST.

## Research gate

**Blocker 1 is open.** The active question is whether the project can produce a semantic model / primitive decomposition of effect-making that is materially distinct and musically consequential rather than a cleaner restatement of established DSP languages, patchers or live-coding systems.

Before adding user-visible semantics, read the current private research status in `krahd/research/projects/audio_scripter/STATUS.md` when access is available. Semantic and articulatory distance are design pressures, not a licence to add convenience syntax.

Do not treat any of the following as pre-authorised contributions: automatic state, arrays/buffers, musical units, parameters, history, feedback constructs, channel polymorphism, MIDI/events, pattern notation, graphical views, live reload, multiple abstraction levels or declarative constraints.

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
bash scripts/build_release.sh --config Release
bash scripts/build_release.sh --config Debug --install
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
- avoid introducing new allocation, blocking, I/O, logging, string construction, container growth or mutex acquisition into per-sample/audio-callback paths;
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

## Language-change rule

Before adding or changing user-visible syntax/semantics:

1. read `STATUS.md`, `docs/ROADMAP.md`, and `docs/RESEARCH.md`;
2. consult `krahd/research/projects/audio_scripter/STATUS.md` and relevant research dossiers when available;
3. identify the musical/semantic problem the change addresses;
4. check the strongest relevant precedent system rather than assuming the feature is absent elsewhere;
5. compare against that system's strongest idiomatic abstraction, not an artificially low-level version;
6. distinguish semantic change from notation/editor/runtime/host convenience;
7. ask whether the proposal changes primitive decomposition or merely reduces articulation;
8. implement the smallest prototype necessary to test the hypothesis;
9. add behavioural tests;
10. update `docs/LANGUAGE_SPEC.md` only when the implementation actually changes.

Large language work is **not pre-authorised by the old roadmap**.

## Testing

After engine/parser changes:

```bash
cmake --build build --target audio_scripter_parser_tests
ctest --test-dir build --output-on-failure
```

After adding/modifying examples:

```bash
python3 tools/validate_scripts.py
```

For performance-sensitive work:

```bash
cmake --build build --target audio_scripter_render_report
./build/audio_scripter_render_report
```

Do not turn a historical benchmark threshold into a correctness claim. Record exact configuration and results when performance matters.

## Documentation boundary

This repository is public.

Public material may include verified implementation facts, source/tests/examples, conservative source-checked research context, and reviewed reproducibility material.

Do **not** add unpublished novelty arguments, detailed competitive judgements, semantic-kernel alternatives, paper drafts, internal theoretical notes, raw reflective-practice material, unreviewed artwork concepts or funding strategy. Those belong in `krahd/research/projects/audio_scripter/` or, for a distinct manuscript, under `krahd/research/academic-writing/`.

## STATUS.md — mandatory upkeep

`STATUS.md` must remain current after any material change. Record current focus/research gate, verified root causes of open technical issues, changes already on `main`, altered next actions, and material implementation/research state that affects other repositories.

Use factual wording and distinguish implemented behaviour, measured evidence and hypotheses.

## Cross-repository administration

`krahd/tom-work-admin` is canonical for global/cross-domain status. Follow `WORK-ADMIN.md` and update `tom-work-admin` in the same work session whenever lifecycle, research direction, major validation state, publication relationship, name/status, or next cross-domain action changes.

Ownership:

- implementation/public docs: `krahd/audio_scripter`;
- private project-centred research: `krahd/research/projects/audio_scripter/`;
- future distinct manuscript(s): `krahd/research/academic-writing/my_papers_<year>/`;
- global status: `krahd/tom-work-admin`.

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