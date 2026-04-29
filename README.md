# audio_scripter 1.1.0

audio_scripter is a JUCE-based, real-time scriptable audio effect plugin (VST3, AU, Standalone).

Write DSP scripts that run **once per audio sample**, manipulate `inL`/`inR`, and write `outL`/`outR` — that's it. No compilation step, no DAW restart: press **Apply** and the new script takes effect immediately.

**Website:** [krahd.github.io/audio_scripter](https://krahd.github.io/audio_scripter/)

## Highlights

- Per-sample script language: arithmetic, user functions, persistent state, and loop controls (`break` / `continue`).
- Lock-free script swap — zero-copy hot reload at runtime.
- 8 DAW-automatable macro knobs `p1..p8` accessible from every script.
- Extended DSP primitives: `lpf1`, `hp1`, `bp1`, `svf`, `env`, `slew`, `delay`, `sat`.
- Cross-format: VST3, AU, Standalone (macOS); Windows VST3 & Standalone via CI.

## Documentation

| Document | Contents |
| --- | --- |
| [Language manual](docs/LANGUAGE_SPEC.md) | Full script language tutorial, reference, and examples |
| [Changelog](docs/CHANGELOG.md) | Release history |
| [Plugin audit](docs/PLUGIN_AUDIT_AND_NEXT_STEPS.md) | Internal release-engineering checklist |

## Build requirements

- CMake 3.22+
- A C++20 compiler (Clang or recent GCC / MSVC on Windows)
- JUCE — fetched automatically via CMake FetchContent, or supply a local checkout

## Quick build & package

```bash
# Build release and package binaries (VST3 / AU / Standalone)
./scripts/build_release.sh --config Release --package

# Build and run validator + parser tests
./scripts/build_release.sh --config Release --tests

# Use a local JUCE checkout
./scripts/build_release.sh --juce-path /path/to/JUCE --config Release --package
```

## Install plugin (macOS)

```bash
# User install with timestamped backup of any existing bundle
./install.sh -b

# System-wide install (requires root)
sudo ./install.sh --system -b

# Convenience: build, install, quit Ableton, open test project
./scripts/build_and_open_als.sh
```

## VS Code

`.vscode/tasks.json` includes a `Build Release + Install VST3` task.

## Tests & validation

```bash
# Validate example scripts against the parser
python3 tools/validate_scripts.py

# Run parser unit tests (after CMake configure)
cmake --build build --target audio_scripter_parser_tests
ctest --test-dir build --output-on-failure
```

## CI and releases

A CI workflow (`.github/workflows/ci.yml`) runs the script validator and parser tests on every push and PR.

A release workflow (`.github/workflows/release.yml`) builds macOS and Windows artifacts and publishes a GitHub Release when you push a `v*` tag:

```bash
git tag v1.1.0
git push origin v1.1.0
```

## Examples

Scripts live in `examples/`. Load any `.ascr` file from the plugin's **Load** button or pick one from the examples dropdown.

Authoring tips:

- Smooth discontinuities with `slew` or `lpf1` to avoid clicks.
- Prefer event-triggered noise bursts over always-on `noise(t * sr)`.
- Stage wet/dry with `mix(dry, wet, p1)` so the effect is always controllable.

## License

This project is released under the [MIT License](LICENSE).

## Contributing

Open issues and PRs against `main`. CI will run the validator and parser tests automatically.
