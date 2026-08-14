# audio_scripter

**Experimental research prototype · current implementation: 0.0.13**

audio_scripter is an experimental system for designing and testing small languages for programmable audio effects. The repository currently contains a JUCE host application/plugin (VST3, AU, Standalone), a per-sample scripting language, an interpreter/runtime, tests, and a library of example effects.

The project is **in active development**. The current language is a research baseline, not a stable API: syntax, semantics, host integration, file formats, and project name may change substantially. It should not yet be treated as production software or as a backwards-compatible plugin platform.

**Website:** [krahd.github.io/audio_scripter](https://krahd.github.io/audio_scripter/)

## Current baseline

The implemented 0.0.13 language uses a direct per-sample model: scripts read `inL`/`inR`, compute, and write `outL`/`outR`. It currently provides:

- arithmetic, comparisons, logical and bitwise operators;
- user-defined functions and bounded control flow;
- persistent `state_` variables;
- eight DAW-automatable macro parameters (`p1`…`p8`);
- DSP primitives including filters, envelopes, slew limiting, delays, saturation, shaping, and noise;
- atomic publication of newly compiled program snapshots while audio processing continues;
- VST3, AU, and Standalone JUCE targets;
- a curated example corpus in `examples/`.

See [the language manual](docs/LANGUAGE_SPEC.md) for the language exactly as it is implemented today.

## Research and design direction

The immediate research task is **language design**, deliberately separated from the design of the VST/AU interface. The plugin is one host for the language, not the definition of the language.

Current work is focused on:

1. establishing a careful prior-art and comparison baseline across programmable audio and music languages;
2. analysing the conceptual cost of expressing representative audio effects in existing systems;
3. defining a host-independent semantic kernel before committing to new syntax;
4. investigating learnability, expressive range, and the distance between musical intention and program expression;
5. retaining the plugin/runtime as an experimental host and evaluation environment.

No novelty claim is made here for programmable plugins, per-sample DSP scripting, hot reload, or live DSP modification. Those capabilities have substantial precedents. Public research notes are in [docs/RESEARCH.md](docs/RESEARCH.md); unpublished analysis and paper development are maintained separately.

The pre-redesign project state is preserved for provenance on branch [`archive/pre-language-redesign-2026-08-14`](https://github.com/krahd/audio_scripter/tree/archive/pre-language-redesign-2026-08-14).

## Development status and known limitations

The current implementation is useful as an experimental baseline, but important engineering work remains:

- `delay()` still performs allocation on first use of a lane in the audio path;
- some fallback state lookup paths perform string/hash work per sample;
- the current `t` representation loses precision in long sessions;
- the language is stereo-oriented and exposes implementation-level state lane identifiers;
- there are parser/behavioural tests, but no complete golden-audio suite or audio-thread allocation guard yet;
- plugin behaviour has not been validated exhaustively across supported hosts.

These limitations are tracked in [STATUS.md](STATUS.md) and [docs/ROADMAP.md](docs/ROADMAP.md).

## Documentation

| Document | Contents |
| --- | --- |
| [Language manual](docs/LANGUAGE_SPEC.md) | Exact reference for the implemented 0.0.13 language |
| [Project status](STATUS.md) | Current development state, limitations, and active work |
| [Roadmap](docs/ROADMAP.md) | Research-first development sequence |
| [Research note](docs/RESEARCH.md) | Public, conservative research context and verified precedents |
| [Developer guide](docs/DEVELOPER_GUIDE.md) | Runtime architecture, DSP primitives, build/release details |
| [Changelog](docs/CHANGELOG.md) | Release history |

## Build requirements

- CMake 3.22+
- A C++20 compiler (Clang or recent GCC / MSVC on Windows)
- JUCE, fetched automatically with CMake FetchContent or supplied locally

## Build and test

```bash
# Build release and package binaries
./scripts/build_release.sh --config Release --package

# Build and run validator + parser tests
./scripts/build_release.sh --config Release --tests

# Use a local JUCE checkout
./scripts/build_release.sh --juce-path /path/to/JUCE --config Release --package
```

Additional validation:

```bash
python3 tools/validate_scripts.py
cmake --build build --target audio_scripter_parser_tests
ctest --test-dir build --output-on-failure
```

## Examples

Scripts in `examples/` include conventional studio effects and less conventional signal-processing sketches. They are primarily a regression, comparison, and language-design corpus; they should not be read as a statement that the current syntax is final.

## License

MIT. See [LICENSE](LICENSE).

## Contributing

Issues and pull requests are welcome. Because the language is being reconsidered at the semantic level, substantial syntax/API additions should be discussed before implementation.