# audio_scripter — Development Roadmap

Status: active research/development · updated 2026-08-14

This roadmap supersedes the earlier feature-driven v0.1 plan. The existing 0.0.13 implementation remains a useful experimental baseline, but new language features are **not** to be added merely because they increase conventional DSP capability. The next phase is to determine what the language should mean before expanding what it can express syntactically.

The pre-redesign repository state is preserved at `archive/pre-language-redesign-2026-08-14`.

## 1. Design constraints

### Separate language from host

The language must not be defined by the current VST/AU user interface. Treat the system as three layers:

1. **Language** — host-independent meanings and expressions.
2. **Runtime** — audio/control/event execution, state, memory, scheduling, compilation/evaluation, safety.
3. **Host adapter** — VST3/AU/Standalone/other integration, parameter exposure, transport, persistence, UI.

The current JUCE plugin is one adapter and one experimental interface.

### Research before feature accumulation

Named state, parameter declarations, arrays/buffers, MIDI, multichannel support, higher-level temporal constructs, and other features may eventually be appropriate. None is presumed to belong in the language until the comparison and benchmark work shows what conceptual problem it solves.

### Public claims remain conservative

Programmable audio plugins, per-sample scripting, dynamic recompilation, in-DAW authoring, live coding, and stateful DSP languages all have substantial precedents. This repository does not claim novelty for those capabilities.

## 2. Phase A — Freeze and provenance

- [x] Freeze pre-redesign state on `archive/pre-language-redesign-2026-08-14` at `d98e3379c9f2b1e21cf8527c31384987917eed58`.
- [x] Reframe `main` as an experimental research prototype.
- [x] Make the public/private research boundary explicit.
- [ ] Keep 0.0.13 runnable while language research proceeds.

## 3. Phase B — State-of-the-art and benchmark gate

No substantial language redesign should be merged before this phase produces a defensible result.

### Prior-art map

Study the strongest relevant systems at the level of semantics and authoring model, not merely feature checklists. The comparison set includes, at minimum:

- REAPER JSFX / EEL2;
- Kronos / Kronos VST;
- Blue Cat Plug'n Script;
- Faust;
- Gen / GenExpr;
- Cmajor;
- mimium;
- ChucK and SuperCollider/JIT approaches where relevant;
- other systems revealed by the continuing literature review.

Detailed unpublished analysis is kept in the private academic-writing repository.

### Effect/challenge corpus

Construct a corpus that spans:

- simple conventional effects;
- stateful and feedback effects;
- tempo/time-dependent processing;
- channel-structural transformations;
- effects requiring histories/buffers;
- signal-dependent temporal operations;
- deliberately unusual effects that expose the limits of fixed commercial plugins.

The purpose is not to demonstrate that other programmable systems are computationally incapable. It is to expose the concepts and implementation machinery required to express each intention.

### Analysis dimensions

For each system/task pair, record at least:

- concepts the programmer must represent explicitly;
- state/memory management burden;
- sample/time/tempo conversion burden;
- channel-routing burden;
- parameter and host-mapping burden;
- feedback representation;
- temporal-history representation;
- amount and locality of boilerplate;
- hidden dependencies and error modes;
- expression size only as a secondary proxy, never as the sole measure of complexity.

## 4. Phase C — Semantic kernel

Derive the smallest host-independent set of concepts that materially reduces the observed representational burden.

Candidate areas to investigate include:

- signals as first-class values;
- musical/physical quantities and units;
- time and tempo as explicit semantic objects;
- scoped state rather than manually numbered implementation lanes;
- direct representation of signal history;
- explicit but concise feedback;
- channel-polymorphic or structural operations;
- composition mechanisms that preserve readability;
- parameters defined semantically and mapped to hosts separately.

These are hypotheses, not commitments.

### Exit criterion

Proceed only if the candidate kernel can make a meaningful set of musically relevant tasks more direct without becoming a disguised library over a conventional general-purpose DSP language.

## 5. Phase D — Minimal prototype and reflective use

- Implement only enough of the candidate kernel to test the central claims.
- Keep syntax deliberately provisional.
- Use the language repeatedly in actual music/sound work.
- Maintain a design log of intention → expression → friction → redesign → musical consequence.
- Preserve failed designs as research evidence.
- Compare equivalent tasks with selected precedent systems.

External participants are not required for formative design, but claims about learnability beyond the designer require external validation.

## 6. Phase E — Engineering credibility

The existing runtime has known issues that must be fixed before strong real-time-safety claims:

- pre-allocate delay/state resources off the audio thread;
- remove per-sample dynamic string/hash fallback paths;
- add an audio-thread allocation guard;
- add deterministic golden-audio tests;
- fix long-session time precision;
- verify behaviour across representative hosts and plugin formats.

These tasks can proceed in parallel when they do not constrain language semantics.

## 7. Phase F — Publication/artistic release

A paper-ready milestone should include:

- a verified state-of-the-art boundary;
- an explicit theoretical framework;
- the benchmark/challenge corpus;
- a language design derived from observed representational problems;
- a runnable reference implementation;
- comparative examples;
- evidence from sustained artistic use;
- appropriately scoped external evaluation where claims require it;
- reproducible technical measurements for implementation claims.

Publication strategy, artwork development, funding strategy, and unpublished novelty arguments are maintained privately.

## 8. Naming

`audio_scripter` is a working name, not a settled identity. Naming review should occur after the semantic centre is clearer and must include project-name, software, domain, app-store, and trademark searches before any rename.