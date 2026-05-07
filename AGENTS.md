# AGENTS.md

Repository instructions for AI coding agents working in this project.

This file is the durable source of truth for GitHub Copilot, OpenAI Codex, Claude Code, and compatible coding agents. Read it before making changes. Follow the most specific applicable instruction when several instruction files exist.

## 1: Non-negotiable rules

- Keep `STATUS.md` accurate at all times.
- `STATUS.md` must exist in the repository root.
- Do not finish a task that changes the project without reviewing and, when needed, updating `STATUS.md`.
- Do not invent project facts. Inspect the repository and record uncertainty explicitly.
- Do not overwrite user work or unrelated changes.
- Do not commit secrets, credentials, tokens, private keys, local environment files, build artefacts, or generated sensitive data.
- Prefer small, focused changes over broad rewrites.
- Preserve existing project style unless explicitly asked to change it.
- Verify meaningful changes with the narrowest reliable command available.
- Do not claim that tests, builds, smoke tests, linters, type-checkers, or manual checks passed unless they were actually run.

## 2: Communication style

Use terse, factual, technical communication. Do not use playful, whimsical, cute, decorative, or filler progress phrases such as "combobulating", "cooking", "thinking...", "working on it", "let me dive in", "I'll get started", or "working my magic".

Allowed status-update style: "Reading files." "Found the issue." "Applying patch." "Tests passed." "Tests failed: <reason>."

No jokes, metaphors, fake enthusiasm, anthropomorphising, or decorative progress messages. Prefer concise present-tense technical updates. Use British English for prose documentation unless the repository consistently uses another variant.

## 3: Standard work loop

1. Read this file and `STATUS.md` before editing.
2. Inspect relevant files, docs, tests, build scripts, and CI workflows.
3. Identify the smallest safe change.
4. Search relevant call sites before changing public APIs, script syntax, DSP primitives, build settings, installer logic, or release automation.
5. Make focused edits.
6. Run relevant verification when possible.
7. Update documentation when behaviour, setup, architecture, commands, or public APIs change.
8. Update `STATUS.md` if project state changed.
9. Report changed files, verification, and remaining issues.

## 4: Project-specific map

### 4.1: Project shape

- Purpose: JUCE-based real-time scriptable audio effect plugin.
- Main runtime surfaces: VST3, AU, Standalone plugin/application, script editor UI, example scripts, release installers.
- Primary languages/frameworks: C++20, JUCE, CMake, Python validation scripts.
- Supported platforms: macOS and Windows build/release targets; Linux primarily for CI-style validation unless otherwise documented.

### 4.2: Important paths

- `README.md`: human-facing overview and build instructions.
- `STATUS.md`: complete current project status report; mandatory upkeep.
- `CMakeLists.txt`: build configuration.
- `Source/`: JUCE plugin source code.
- `examples/`: `.ascr` effect scripts.
- `docs/LANGUAGE_SPEC.md`: script language reference.
- `docs/DEVELOPER_GUIDE.md`: architecture and release details.
- `docs/CHANGELOG.md`: release history.
- `tools/validate_scripts.py`: example script validator.
- `scripts/build_release.sh`: release build/package helper.
- `install.sh`: plugin installation helper.
- `.github/workflows/`: CI and release automation.

### 4.3: Safety invariants

- Real-time audio processing must avoid allocations, blocking work, locks, and unbounded operations on the audio thread.
- Script swaps must remain safe for live audio use.
- DSP primitives must avoid undefined behaviour, runaway feedback, NaNs, and unbounded state growth.
- Installer scripts must preserve backup behaviour and avoid deleting user plugin bundles without explicit safeguards.
- Example scripts should be validated before being treated as working examples.

## 5: STATUS.md maintenance

`STATUS.md` is mandatory project state, not optional documentation.

Required timestamp line near the top:

```text
Last updated: YYYY-MM-DD HH:MM
```

Rules:

- Use exactly that format with 24-hour time.
- Use the user's local timezone; if no other timezone is specified, use `America/Montevideo`.
- Duplicate the exact same `Last updated` line as the final line at the bottom of `STATUS.md`.
- Update both lines in the same edit.
- Treat missing, stale, malformed, or mismatched timestamp lines as a blocking documentation error for tasks that change project state.

`STATUS.md` must be a complete current snapshot, not a changelog. Include relevant sections for purpose, current implementation state, active focus, architecture, setup/run instructions, configuration, important files, recent changes, tests, risks, pending tasks, next steps, longer-term steps, and decisions.

## 6: Diagrams in STATUS.md

Include useful inline SVG architecture and flow diagrams when the project structure is meaningful enough. Keep text inside boxes and canvas bounds. Keep arrows out of unrelated boxes and labels. Prefer generous spacing and simple SVG primitives. Update diagrams only when structural or flow changes warrant it.

## 7: Coding, validation, and release rules

- Follow existing style, naming, layout, and architecture.
- Preserve the script language and public behaviour unless explicitly asked to change them.
- Add/update tests for behavioural changes when a test pattern exists.
- Do not delete failing tests to make a suite pass.
- Keep build/release/version references aligned across docs, CMake/project metadata, scripts, and `STATUS.md`.
- Do not create tags, releases, or publish packages unless explicitly requested.

Typical validation commands:

```bash
python3 tools/validate_scripts.py
./scripts/build_release.sh --config Release --tests
cmake --build build --target audio_scripter_parser_tests
ctest --test-dir build --output-on-failure
```

## 8: Final response requirements

When finishing a task, report concisely: what changed, files changed, verification commands and results, whether `STATUS.md` was updated, and remaining issues or follow-up work.
