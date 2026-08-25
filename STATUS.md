# audio_scripter — Project Status

Last updated: 2026-08-25 · implementation baseline 0.0.13

## Status

**Active experimental research/development project. Blocker 1 remains OPEN.**

The current 0.0.13 implementation remains a baseline for language-design research, not the target language specification. The pre-redesign state is frozen at:

- branch: `archive/pre-language-redesign-2026-08-14`
- commit: `d98e3379c9f2b1e21cf8527c31384987917eed58`

The project name `audio_scripter` remains provisional.

## Current semantic experiment

Branch `research/lifecycle-spike` contains the first executable semantic spike authorised by the private research programme.

It does **not** change the `.ascr` grammar, parser, JUCE plugin semantics or C++ runtime.

The experiment tests whether a persistent stateful transformation benefits from explicit, independently variable lifecycle policies for:

- input participation;
- internal-state evolution;
- output participation;
- history accumulation;
- observation evolution.

Files:

- `tools/lifecycle_spike.py`
- `tests_python/test_lifecycle_spike.py`

Current reference cases:

- normal operation;
- tail: block new input while state continues evolving and remains audible;
- freeze: block input and freeze state;
- silent evolution: continue input/state while suppressing output;
- history-only accumulation;
- resume without recreating transformation identity.

Local validation on 25 August 2026: **5 behavioural tests passed** after correcting the Python dynamic-import test harness.

This validates internal consistency of the spike only. It is **not evidence of semantic novelty or superiority**.

## Research gate

The general viability of audio/music programming-language research is already established. The active gate is whether this project can produce a language whose semantic model and primitive decomposition are materially distinct and useful rather than a cleaner restatement of existing DSP, patcher, live-coding, FRP or functional-signal abstractions.

Current private research has rejected/demoted several broader candidate cores:

- declarative/constraint as primary core: rejected;
- behaviour/pattern as primary core: demoted;
- generic first-class relation as novelty carrier: rejected after FRP/higher-order DSP comparison.

The current provisional survivor is a **persistent transformation + participation/lifecycle contract**. Strong precedents already exist for every component separately, including SuperCollider NodeProxy/JITLib and Max/MSP routing/muting. The experiment must determine whether the decomposition itself provides useful semantic leverage.

Detailed unpublished analysis remains in:

`krahd/research/projects/audio_scripter/`

## Current implementation baseline

The existing `.ascr` language remains the small imperative per-sample baseline documented in `docs/LANGUAGE_SPEC.md`. No public language-semantic change is implied by the lifecycle spike.

Independent baseline engineering issues remain:

- `delay()` can allocate on first lane use on the audio path;
- fallback stateful-builtin paths can perform per-sample string/hash work;
- `t` precision degrades in long sessions;
- no automated audio-thread allocation guard;
- no comprehensive golden-audio regression suite;
- incomplete cross-host/plugin-format validation.

## Next experiment actions

1. Add lifecycle transition/invariant tests.
2. Add musical-time policy scheduling without syntax design.
3. Test a second structurally different stateful process.
4. Make history policy audibly consequential.
5. Compare the same lifecycle cases directly with idiomatic SuperCollider NodeProxy/JITLib and Max/MSP constructions.
6. Reject/demote the candidate if it proves to be only packaging around ordinary gates/muting/routing.
7. Only if it survives, design the smallest semantic AST/IR before considering user-facing syntax.

## Ownership

- implementation/tests/examples/releases: this repository;
- private theory/design/prior-art/publication strategy: `krahd/research/projects/audio_scripter/`;
- future distinct manuscripts: `krahd/research/academic-writing/my_papers_<year>/`;
- global state/relationships: `krahd/tom-work-admin`.
