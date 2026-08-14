# audio_scripter — Project Status

Last updated: 2026-08-14 · implementation baseline 0.0.13

## Status

**Active experimental research/development project.**

The current 0.0.13 implementation is now treated as a **baseline for language-design research**, not as the target language specification. The pre-redesign state is frozen at:

- branch: `archive/pre-language-redesign-2026-08-14`
- commit: `d98e3379c9f2b1e21cf8527c31384987917eed58`

The project name `audio_scripter` is also provisional and will be reviewed against existing names, trademarks, projects, and the eventual conceptual identity of the work.

## Project purpose

The project investigates how a language for programmable audio effects can be easy to learn and use while remaining expressive enough to support effects and transformations that are cumbersome to construct with conventional plugins.

The language and the plugin interface are now explicitly separate design problems:

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

## Research decision — 2026-08-14

**Blocker 1: provisionally passed.**

The work is considered a viable research programme provided that the next stage can demonstrate a meaningful class of musical/audio intentions for which existing programmable audio systems impose unnecessarily high conceptual or representational cost, and that a new language design can reduce that cost without collapsing expressive range.

The current theoretical centre under investigation is the relation between **semantic distance** and **articulatory distance** in an audio programming language: the distance between what a musician intends, the concepts the language makes available, and the concrete expressions required to invoke them.

This framing and all unpublished novelty analysis are maintained privately in `krahd/academic-writing`.

## Active work

### Blocker 2 — prior-art and benchmark validation

Before substantial new language implementation:

1. complete a systematic comparison of relevant programmable-audio systems and research languages;
2. construct a representative effect/challenge corpus;
3. analyse the implementation concepts each system forces the author to manage;
4. identify candidate reductions in semantic/articulatory distance;
5. test those reductions through small semantic prototypes before fixing syntax.

Only after this gate should the new language kernel be committed.

### Engineering credibility work

Independent of language redesign, the existing baseline still has concrete technical issues:

- `delay()` allocates on first use of a lane on the audio path;
- non-literal stateful-builtin fallbacks can perform per-sample string/hash work;
- `t` is represented in a way that loses sub-sample precision during long sessions;
- there is no allocation-guard test for the audio callback;
- there is no comprehensive golden-audio numerical regression suite;
- cross-host/plugin-format validation remains incomplete.

These are implementation facts, not research contributions, but must be resolved before strong real-time-safety claims are made.

## Public/private boundary

This public repository contains implementation, examples, tests, verified technical documentation, and conservative public research context.

Unpublished material belongs in the private `krahd/academic-writing` repository, including:

- novelty claims still under review;
- detailed prior-art matrices and evaluative judgements;
- semantic/articulatory-distance theory development;
- paper arguments and venue strategy;
- artwork concepts and unpublished experiments;
- funding and PhD-programme planning.

## Next actions

1. Finish and audit the private state-of-the-art map.
2. Define the effect/challenge benchmark corpus.
3. Compare representative tasks across the strongest relevant systems.
4. Derive a candidate host-independent semantic kernel.
5. Prototype the kernel minimally and use it in sustained musical practice.
6. Revisit syntax only after the semantic model survives these tests.
7. In parallel, repair the baseline runtime's real-time-safety gaps.

See `docs/ROADMAP.md` for the public development sequence.