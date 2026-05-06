# Agent Instructions — audio_scripter

These instructions apply to all AI coding agents working in this repository
(GitHub Copilot, OpenAI Codex, Claude, and compatible tools).

---

## Repository overview

`audio_scripter` is a JUCE-based audio plugin (VST3 + AU) that interprets a
domain-specific scripting language (`.ascr`) at audio-thread rate. The engine
is intentionally minimal and allocation-free on the hot path.

Key directories:

| Path | Purpose |
|---|---|
| `Source/` | Plugin C++ source (engine, parser, tokeniser, editor, processor) |
| `examples/` | Bundled `.ascr` effect scripts |
| `tests/` | CTest-based C++ unit tests |
| `tests_python/` | Python validation tests for example scripts |
| `tools/` | Offline render / benchmark tool (`RenderEffectReport.cpp`) |
| `docs/` | Language spec, developer guide, changelog |
| `scripts/` | Build and packaging shell scripts |

---

## Build

```bash
# Release build (VST3 + AU)
bash scripts/build_release.sh --config Release

# Debug build + install
bash scripts/build_release.sh --config Debug --install

# Run tests
ctest --test-dir build --output-on-failure
```

CMake 3.24+ and the JUCE 7 FetchContent dependency are required. The build
produces artefacts under `build/audio_scripter_artefacts/`.

---

## Code conventions

- C++17; no exceptions; no RTTI in audio-thread code.
- The audio thread must be **allocation-free**. Never call `new`, `malloc`,
  `std::vector::push_back` that may reallocate, or any JUCE allocating API
  (e.g. `juce::String` construction from literals) inside
  `ScriptEngine::processBlock` or any function it calls per-sample.
- Prefer `float` arithmetic; avoid `double` in the engine hot path.
- Use `juce::String` only outside the audio thread (editor, parser setup).
- New built-in functions belong in `ScriptEngine.cpp` alongside existing ones
  (`lpf1`, `bp1`, `svf`, `env`, `slew`, `delay`, …).
- Parser changes belong in `ScriptParser.cpp / .h`; AST node types are defined
  there.
- Keep `Source/Constants.h` in sync when bumping the plugin version.

---

## Testing

- Run the CTest suite after any engine or parser change.
- After adding or modifying `.ascr` examples, run:
  ```bash
  python3 tests_python/test_validate_scripts.py
  ```
- For performance-sensitive changes, build the render/benchmark tool and check
  realtime factors:
  ```bash
  cmake --build build --target audio_scripter_render_report
  ./build/audio_scripter_render_report
  ```
  All example effects should achieve a realtime factor ≥ 1.0 at 48 kHz /
  256-sample blocks.

---

## STATUS.md — mandatory upkeep

**`STATUS.md` must be kept up to date at all times.**

After every non-trivial change (bug fix, optimisation, new feature, refactor,
or noteworthy investigation finding), update `STATUS.md` as part of the same
commit or work session. The file must reflect:

- The **current focus** — what is actively being worked on.
- The **root cause** of any open bug, once identified.
- A summary of **changes already on `main`** (add a row to the table).
- Any new or changed **files of interest** or **diagnostic outputs**.
- The **last updated** timestamp at the top of the file, in the format:

  ```
  Last updated: YYYY-MM-DD HH:MM
  ```

  Use the local wall-clock time (24-hour). Never leave the timestamp stale.

Agents must not close a task or mark work complete without first verifying that
`STATUS.md` accurately describes the current state of the project.

---

## Pull-request / commit hygiene

- Commits should be atomic and focused on a single concern.
- Commit messages: imperative mood, ≤ 72 characters on the subject line.
- Do not commit build artefacts, `.DS_Store`, or files under `build/`.
- Do not force-push to `main`.

---

## Safety rules

- Do not modify or delete files under `build/` — that directory is generated.
- Do not run `rm -rf` without explicit user confirmation.
- Do not push to remote branches without explicit user instruction.
- When in doubt about a destructive action, ask before proceeding.
