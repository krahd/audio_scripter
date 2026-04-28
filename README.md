# audio_scripter 1.0.8

audio_scripter is a JUCE-based, real-time scriptable audio effect plugin (VST3, AU, Standalone).

Status: Release v1.0.8 — GitHub release: https://github.com/krahd/audio_scripter/releases/tag/v1.0.8

## Highlights

- Script language for per-sample DSP with arithmetic, functions, and persistent state.
- Lock-free script swap architecture for zero-copy updates at runtime.
- DAW-automatable macro controls `p1..p8` mapped to plugin parameters.
- Cross-format builds: VST3, AU, Standalone (macOS), Windows VST3 & Standalone supported in CI.

## Build requirements

- CMake 3.22+
- A C++20 compiler (Clang or recent GCC on Linux/Windows)
- JUCE (optional local checkout) — can use FetchContent to download JUCE during configure

## Quick build & package (recommended)

From the repository root:

```bash
# Build release and package binaries (VST3/AU/Standalone)
./scripts/build_release.sh --config Release --package

# Build and run validator/parser tests
./scripts/build_release.sh --config Release --tests
```

If you have a local JUCE checkout and want to use it:

```bash
./scripts/build_release.sh --juce-path /path/to/JUCE --config Release --package
```

## Install plugin (macOS)

- Install into the current user's audio plugin folders, creating a timestamped backup of any existing bundle:

```bash
./install.sh -b
```

- Install system-wide (requires root) and create a backup:

```bash
sudo ./install.sh --system -b
```

- Convenience script (build, install, quit Ableton, open test Project):

```bash
./scripts/build_and_open_als.sh
```

## VS Code

- There are useful tasks in `.vscode/tasks.json`, including:
  - `Build Release + Install VST3` — builds Release and installs the plugin (backs up existing bundle)

## Tests & validation

- Validate example scripts:

```bash
python3 tools/validate_scripts.py
```

- Parser unit tests (after CMake configure):

```bash
cmake --build build --target audio_scripter_parser_tests
ctest --test-dir build --output-on-failure
```

## CI and releases

- A CI workflow (`.github/workflows/ci.yml`) runs the script validator and parser tests on push/PR.
- A release workflow (`.github/workflows/release.yml`) builds macOS/Windows artifacts and publishes a GitHub Release when you push a tag `v*` (e.g. `v1.0.8`).

To create a release from your machine:

```bash
git tag v1.0.8
git push origin v1.0.8
# The release workflow will build and publish artifacts automatically.
```

NOTE: The CI release currently packages artifacts but does not perform Apple codesigning/notarization — see Notes below.

## Signing & notarization (summary)

- For macOS distribution you should codesign and notarize binaries. High-level steps:
  1. Obtain Apple Developer ID Application and Developer ID Installer certs and an App Store Connect API key.
 2. Import certs into a keychain on your build machine or CI runner and set key partition list for `codesign`.
 3. Sign embedded frameworks/dylibs, then the main executable(s), then the plugin bundle(s).
 4. Package as zip/pkg and submit to Apple notarization (`xcrun notarytool` recommended).
 5. Staple the notarization ticket (`xcrun stapler staple`).

See `docs/PLUGIN_AUDIT_AND_NEXT_STEPS.md` for a prioritized release-engineering checklist and suggested automation
steps for CI.

## Examples

- Scripts live in the `examples/` directory. Use `tools/validate_scripts.py` to check example compatibility.

## Changelog

- See [docs/CHANGELOG.md](docs/CHANGELOG.md) for the release history. This repo has just been bumped to `1.0.8`.

## Important notes & known gaps

- No LICENSE file is present in the repository. Please add a `LICENSE` (e.g. MIT, Apache-2.0) to make reuse terms clear.
- The codebase contains an internal audit in `docs/PLUGIN_AUDIT_AND_NEXT_STEPS.md` describing parser/runtime hygiene and recommended
  work to reach a fully-featured, test-backed release. Read it before making large language/runtime refactors.

## Contributing

- Open issues and PRs against `main`. The CI will run validator and parser tests on PRs.

---

Files of interest:

- [Build script](scripts/build_release.sh)
- [Install helper](install.sh)
- [Release workflow](.github/workflows/release.yml)
- [Audit & next steps](docs/PLUGIN_AUDIT_AND_NEXT_STEPS.md)
