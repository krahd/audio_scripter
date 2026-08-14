# audio_scripter — Project Status

Last updated: 2026-08-14 · implementation baseline 0.0.13

## Status

**Active experimental research/development project.**

The current 0.0.13 implementation is treated as a **baseline for language-design research**, not as the target language specification. The pre-redesign state is frozen at:

- branch: `archive/pre-language-redesign-2026-08-14`
- commit: `d98e3379c9f2b1e21cf8527c31384987917eed58`

The project name `audio_scripter` is provisional and will be reconsidered against existing names and the eventual conceptual identity of the language.

## Project purpose

The project investigates whether a language for programmable audio transformations can combine a low initial conceptual burden with a high expressive ceiling, while making musically meaningful relations more direct to formulate than they are in the current baseline.

The language and plugin interface are explicitly separate design problems:

- **language:** semantics, syntax, temporal/state abstractions, parameters, channels, composition, learnability, expressive cost;
- **runtime:** compilation/evaluation, state, memory, scheduling, safety, audio/control/event interfaces;
- **host adapter:** VST3/AU/Standalone/other host integration, parameter mapping, transport, UI, persistence.

The existing JUCE plugin remains an experimental host. VST-specific constraints should not define the language semantics.

## Current implementation baseline

The existing language is a small imperative, per-sample scripting language. It currently supports:

- `inL`/`inR` → `outL`/`outR` per-sample execution;
- scalar floating-point values;
- arithmetic, comparison, logical, and bitwise operators;
- `if`, `while`, two `for` forms, `break`, and `continue`;
- user-defined functions;
- persistent variables through the `state_` prefix;
- eight normalised macro parameters, `p1`…`p8`;
- built-in filters, delay, envelope, slew, saturation, shaping, oscillation/noise helpers;
- atomic publication of compiled program snapshots;
- 24 embedded example effects.

See `docs/LANGUAGE_SPEC.md` for the exact implemented baseline.

## Research gate

**Blocker 1: OPEN.**

The general scholarly viability of audio/music programming-language research is not the question. The active gate is whether this project can produce a language whose **semantic model and primitive decomposition are materially distinct and useful**, rather than a cleaner or shorter restatement of existing DSP languages and patchers.

Current private research is examining alternative programming paradigms and effect ontologies, not only textual DSP syntax. Semantic and articulatory distance are being used as design pressures as well as later evaluation concepts.

Detailed unpublished comparative analysis remains in the private workspace:

`krahd/research/projects/audio_scripter/`

No substantial new public language semantics should be committed until a candidate design survives this gate.

## Active research work

Before substantial language implementation:

1. compare different primitive ontologies across programmable DSP, patchers, live-coding/pattern systems and declarative approaches;
2. construct a small diagnostic effect/transformation challenge corpus;
3. analyse each relevant precedent using its strongest idiomatic abstraction;
4. design multiple competing semantic kernels before settling syntax;
5. test whether candidates change solution structure rather than line count;
6. require inspectability, composability and non-trivial artistic utility;
7. search for counterexamples/precedents after each design iteration.

### Engineering credibility work

Independent of language redesign, the existing baseline still has concrete technical issues:

- `delay()` allocates on first use of a lane on the audio path;
- non-literal stateful-builtin fallbacks can perform per-sample string/hash work;
- `t` is represented in a way that loses precision during long sessions;
- there is no allocation-guard test for the audio callback;
- there is no comprehensive golden-audio numerical regression suite;
- cross-host/plugin-format validation remains incomplete.

These are implementation facts, not research contributions, but must be resolved before strong real-time-safety claims are made.

## Public/private boundary

This public repository contains implementation, examples, tests, verified technical documentation and conservative public research context.

Unpublished research material belongs in:

`krahd/research/projects/audio_scripter/`

That includes detailed prior-art matrices/evaluative judgements, language-ontology experiments, novelty analysis, paper strategy, raw reflective-practice material, unpublished artwork concepts and funding/graduate-research planning.

When a paper becomes a distinct publication object, its manuscript belongs under `krahd/research/academic-writing/my_papers_<year>/` and should link to the project dossier rather than duplicate it.

## Next actions

1. Design three competing semantic kernels in the private research workspace.
2. Define a small diagnostic challenge set that stresses time, history, feedback, relation, condition and changing behaviour/topology.
3. Compare those challenges against the strongest relevant existing systems.
4. Prototype only the minimum machinery needed to test surviving semantic hypotheses.
5. Use surviving candidates in sustained sonic/artistic practice.
6. In parallel, repair baseline runtime real-time-safety/test gaps where those repairs do not prejudge future semantics.

See `docs/ROADMAP.md` for the public development sequence.