# audio_scripter — Public Research Note

Updated: 2026-08-14

This document records only **public, conservative, verified context** for the project. Detailed novelty analysis, unpublished theory development, paper drafts, artistic experiments, and funding strategy are maintained privately.

## Current research question

audio_scripter is being reconsidered as a language-design research project rather than simply a scriptable plugin.

The working question is:

> How should a programming language for audio effects reduce the distance between a musician's sonic intention and the program needed to realise it, while retaining sufficient computational and artistic expressivity?

The present 0.0.13 syntax is an experimental baseline. It is not assumed to be the answer.

## What is not claimed as novel

The project does **not** claim novelty for any of the following by themselves:

- user-programmable audio effects;
- writing DSP inside or alongside a DAW;
- per-sample audio programming;
- live or dynamic recompilation;
- hot-swapping DSP code;
- automatically exposing parameters to a host;
- textual DSP languages;
- stateful low-level audio programming.

These capabilities have substantial historical and contemporary precedents.

## Verified comparison set

The continuing review includes at least the following systems.

### REAPER JSFX / EEL2

REAPER's JSFX system lets users create audio-oriented effects in EEL2. JSFX code can run per sample, access audio channels directly, define parameters, process MIDI, use memory/FFT facilities, interact with the host, and draw custom interfaces.

- https://www.reaper.fm/sdk/js/js.php
- https://www.reaper.fm/sdk/js/basiccode.php
- https://www.reaper.fm/sdk/js/vars.php

### Kronos VST

Vesa Norilo's *Kronos VST – The Programmable Effect Plugin* (DAFx 2013) describes a VST3 audio effect programmable on the fly by the user, using a high-level functional language and JIT compilation. It is an essential precedent for any claim concerning programmable effects in a plugin host.

- https://www.dafx.de/paper-archive/details/hN1BHTZ28vfUIGXp484wUg

### Blue Cat Plug'n Script

Blue Cat's Plug'n Script is an audio/MIDI scripting plugin and application intended to let users build custom effects or instruments without leaving the DAW, and can export scripts as independent plugins.

- https://www.bluecataudio.com/Doc/Product_PlugNScript/

### Faust

Faust is a functional DSP specification language for synthesis and audio processing. Its architecture system explicitly separates DSP descriptions from audio drivers and controllers, allowing one DSP program to target many environments and plugin formats.

- https://faustdoc.grame.fr/manual/introduction/
- https://faustdoc.grame.fr/manual/architectures/

### Gen / GenExpr

Cycling '74's Gen provides sample-level DSP authoring; GenExpr is its implementation-agnostic expression language and is available through Gen patchers/codebox.

- https://docs.cycling74.com/userguide/gen/gen_genexpr/

### Cmajor

Cmajor is a language/runtime for portable audio DSP. It distinguishes programs, processors, graphs, and patches; patches can be loaded by audio hosts and edited/rebuilt while running. Its documentation explicitly targets real-time safety and familiar procedural syntax.

- https://cmajor.dev/
- https://cmajor.dev/docs/LanguageReference
- https://cmajor.dev/docs/GettingStarted

### mimium

mimium is a programming language for low-level musical DSP. Version 3 introduced live state updating that can preserve delay/feedback state while code is recompiled and replaced.

- https://mimium.org/en/docs/user_guide/livecoding/
- https://mimium.org/en/docs/releasenotes/v3/

### Live-coding and musical-language research

The project also draws context from live coding and musical programming-language research where relevant. For example, Aaron, Blackwell, Hoadley and Regan's NIME 2011 paper *A Principled Approach to Developing New Languages for Live Coding* proposed a three-tier architecture around a Common Music Runtime, allowing multiple interfaces over shared musical processes.

- https://www.nime.org/proc/nime2011_aaron/index.html

## Architecture principle

One conclusion is already sufficiently supported to guide engineering work: **the language should be separable from any one plugin interface**.

The project therefore distinguishes:

1. a host-independent language;
2. an execution/runtime layer;
3. host adapters and interfaces.

This principle has clear precedents in systems such as Faust and Cmajor. It is an architectural requirement for the present research, not itself a novelty claim.

## Evaluation direction

The next public research milestone is not a larger feature list. It is a benchmark and comparison corpus that asks what a programmer must explicitly represent to realise particular audio intentions across different systems.

Candidate dimensions include state management, temporal representation, feedback, channels, host mapping, physical/musical units, hidden dependencies, and the amount of implementation machinery required for conceptually simple effects.

The theoretical framework and resulting hypotheses are under active development and should not yet be treated as established findings.

## Citation discipline

Before any paper submission:

- verify all bibliographic metadata against primary or publisher sources;
- distinguish product documentation from peer-reviewed research;
- avoid unsupported priority/novelty claims;
- preserve historical systems that materially narrow the contribution;
- treat comparisons as evidence to be demonstrated, not marketing claims.