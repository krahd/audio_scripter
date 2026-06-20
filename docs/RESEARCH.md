# Research positioning — audio_scripter

> Working notes for a paper. Draft; citations need a final pass before
> submission (see §8). Companion to [ROADMAP.md](ROADMAP.md).

## 1. Contribution (one sentence)

audio_scripter treats **writing a per-sample DSP effect as an expressive musical
gesture**: a performing musician authors, edits, and reloads their own effects
*inside the DAW they already work in*, making effect *design* part of the
creative act rather than a separate, prior, programmer's task. What makes that
feasible for a musician (rather than a DSP engineer) is a **deliberately tiny
language** with a **direct `inL`/`inR` → `outL`/`outR` mental model**.

The claim is **not** "DSP code can be changed in a running host." That capability
is no longer distinctive — Faust's `faust2clap` dynamic mode (2026) already
permits runtime DSP modification without interrupting the host, with parameter
identity and automation preserved across reloads (§3). Zero-interruption
hot-swap is therefore treated here as **necessary infrastructure (table
stakes)**, not as the contribution.

The contribution is about **who** writes the effect, **when**, **where**, and at
**what conceptual cost**: a musician, in-flow, in-place, against a language small
enough to hold in your head — versus a full DSP specification language and
compiler ecosystem. The language, interface, and runtime are designed end-to-end
to make that loop fast, low-floor, and learnable.

## 2. Why this is a research contribution, not just a tool

Three threads from the literature meet here and have not, to our knowledge, been
combined as an *in-DAW, per-sample, hot-swapped, musician-authored* effect system
evaluated as an expressive practice:

1. **Live coding** treats writing code as performance (Collins, McLean,
   Rohrhuber & Ward 2003; the TOPLAP ethos; Sonic Pi, ChucK, Gibber, ixi lang).
   But live-coding environments are typically *standalone* and *note/pattern*
   oriented — they generate or sequence sound. audio_scripter instead inserts the
   coding gesture into the *signal-processing* path of an ordinary mixing/
   production session.
2. **End-user / appropriable instruments.** NIME has a sustained interest in
   instruments that performers *appropriate* and reshape, and in code as
   expressive material (Zappi & McPherson 2014 on appropriation; Magnusson on
   "sonic writing" and screen-based instruments). audio_scripter makes the
   *effect* — usually a fixed, vendor-authored black box — the appropriable
   surface.
3. **End-user DSP languages** (Faust; Max Gen~/`gen~`; SOUL / Cmajor) give
   non-systems-programmers access to sample-rate DSP, and they are converging on
   in-host live modification — Faust's `faust2clap` dynamic mode (2026) reloads
   interpreted DSP without interrupting the host and preserves parameter
   identity/automation across reloads. audio_scripter does not compete on power
   or on the *fact* of hot reload; it competes on **conceptual size** — a
   language a musician can fully learn in an afternoon — and on **locus**, the
   authoring loop living inside the production tool.

**Gap:** existing systems optimise for *power* and generality (Faust, Cmajor —
full DSP specification languages and compiler/runtime ecosystems) or for
*standalone performance* (ChucK, Sonic Pi). None pairs a **minimal, immediately
graspable per-sample language** with **in-DAW authoring** and frames/evaluates
the musician's act of writing the effect as the unit of expression. The
distinction is **simplicity + locus**, not the existence of hot reload.

## 3. Related systems (and how we differ)

Hot reload is listed for completeness but is *not* our axis of difference — it is
becoming common. The axes that matter are **conceptual size (floor)** and
**locus**.

| System | Domain | Authoring locus | Hot reload | Conceptual size |
| --- | --- | --- | --- | --- |
| Faust | Functional DSP → plugin | Separate toolchain | Static **or** dynamic (faust2clap 2026, params preserved) | Large (full DSP language + ecosystem) |
| Max Gen~ | Patching + gen code | Inside Max patcher | Recompile-ish | Medium–large |
| SOUL / Cmajor | DSP language/runtime | Separate editor | JIT reload | Medium–large |
| ChucK | Strongly-timed lang | Standalone / miniAudicle | Add/replace shreds | Medium |
| Sonic Pi | Live-coding music | Standalone IDE | Yes | Small (but synth/FX graph, not per-sample) |
| SuperCollider | Synthesis lang | Standalone | Re-eval | Large |
| **audio_scripter** | **Per-sample effect scripting** | **Inside the DAW, as a plugin** | **Lock-free, zero-interruption** | **Tiny, by design (`inL`/`inR` → `outL`/`outR`)** |

Our differentiators are the **locus** (a plugin in the user's existing DAW
session, not a separate environment) and the **deliberately tiny language** tuned
for a musician rather than a DSP engineer — a direct per-sample model with no
graph, no signal algebra, no build step. The lock-free zero-interruption swap is
what makes the *gesture* fluid, but it is enabling infrastructure that the field
(e.g. faust2clap dynamic mode) is converging on; we do not rest the contribution
on it.

### The closest systems, precisely

These three demand careful, honest positioning — overclaiming against any of them
weakens the paper.

- **Cmajor** is the heaviest overlap: it explicitly targets "write DSP, run it in
  a DAW, hot-reload it," and it is broader, more industrial, and more powerful
  than audio_scripter. **We do not compete on generality and should never imply we
  do.** The honest distinction is *ergonomic and structural*: Cmajor expects a
  separate patch/project, a VS Code-centric workflow, a C-family
  processor/graph model, and an export/packaging pipeline. audio_scripter is a
  single musician-facing plugin — one text field, a per-sample script, Apply —
  with no project, no external editor, and no export step. The claim is "radically
  simpler and effect-first," not "better DSP."
- **Gen~** is the key precedent for *sample-level authoring with immediate
  auditory feedback*; audio_scripter must **not** claim to have invented that. The
  difference is locus and prerequisite literacy: Gen~ lives inside Max / Max for
  Live and assumes Max fluency (its floor is low *for Max users*). audio_scripter
  makes a different ergonomic bet — a tiny *textual* DSP surface embedded directly
  as a conventional VST3/AU effect, available to a musician who has never opened
  Max.
- **ChucK** is a **literature ancestor, not a market competitor.** Cite it for
  on-the-fly / strongly-timed musical coding as performance — not as a plugin-
  effect authoring tool, which it is not.

## 4. Hot-swap: enabling infrastructure (not the contribution)

For "editing the effect" to be a *musical* action it must be **instantaneous and
safe**: no dropout, no glitch, no crash when a half-finished script is applied.
This is a requirement, not a novelty — Faust's dynamic mode and others provide
comparable in-host reloading. We describe our mechanism here because it is the
basis of the systems *measurements* (§6.2) and because correctness here is a
publication prerequisite, not because it distinguishes the work. audio_scripter
uses a lock-free publish model:

- Scripts are parsed/validated **off** the audio thread; a successful compile is
  published as an immutable `shared_ptr<const CompiledProgram>` via
  `std::atomic_store`.
- The audio thread reads the current snapshot with `std::atomic_load` and runs
  it; an in-flight block always finishes against a consistent program.
- Sample-rate and state-reset are atomic *requests* consumed at block boundaries;
  runtime DSP state stays audio-thread-owned.
- Failed compiles never reach the audio thread; the previous effect keeps
  playing while the musician fixes the error.

This underpins the **systems** half of the evaluation (§6.2). Note: the current
`delay()` path still allocates on first use on the audio thread — making the
RT-safety claim *provably* true is Phase 1 of the roadmap and a prerequisite for
publication. A useful systems comparison is against Faust's dynamic-mode
interpreter on the same effects (reload glitch, parameter-preservation behaviour,
per-sample cost), positioning audio_scripter as the *small-language* point in
that design space rather than a faster or more capable one.

## 5. Design principles (low floor, expressive ceiling)

- **One mental model:** the script runs once per sample; read `inL/inR`, write
  `outL/outR`. No graph, no callback zoo, no build step.
- **Minimal surface, on purpose.** A small, audio-centric builtin set
  (filters, envelopes, delay, saturation, shaping) keeps the floor low and is
  itself the pedagogy argument — but the ceiling must be raised enough to be
  *artistically* satisfying. Hence v0.1 adds **named state, declared parameters,
  and arrays/buffers** (wavetables, FIR) so musicians can build real instruments-
  worth of effect, not toys (see [ROADMAP.md](ROADMAP.md) Phase 2).
- **Feedback is immediate.** Inline errors and live signal display close the
  write → hear loop to seconds, which is what turns editing into a gesture.
- **It lives where the music is made** — a standard VST3/AU/standalone plugin, so
  the practice fits existing workflows instead of replacing them.

## 6. Evaluation plan

The contribution is socio-technical, so the evaluation has two halves.

### 6.1 Musician-led authoring study (qualitative)

- **Participants:** practising musicians/producers spanning coding experience
  (none → fluent), recruited to reflect the target users.
- **Task:** within a single session, modify a provided effect and then author a
  new one to a creative brief, hot-swapping live over a backing track.
- **Data:** screen + audio capture; think-aloud; semi-structured interview;
  edit-loop telemetry (time-to-first-sound, edit→apply latency, error→fix time,
  number of hot-swaps per minute). Analyse with thematic analysis around
  *appropriation*, *flow*, and *expressivity*.
- **Questions:** Does in-flow effect coding feel like an expressive gesture? What
  is the real floor for non-coders? Where does the minimal language frustrate vs.
  liberate?

### 6.2 System characterisation (quantitative)

- **RT-safety:** automated proof of zero audio-thread allocation/locks across the
  example corpus (allocation-guard harness, ROADMAP Phase 1).
- **Hot-swap latency:** measured glitch-free swap (samples of audible
  discontinuity = 0) under load.
- **Throughput:** per-effect realtime factor and p99 block time; baseline for the
  v0.2 bytecode-VM comparison and, ideally, against a Faust-compiled equivalent.

### 6.3 Reproducibility
All figures regenerated from a clean checkout by a CI job; golden-audio set and
benchmark scripts committed (ROADMAP Phase 3).

## 7. Target venues

- **NIME** — appropriation / code-as-expression framing; the musician-authored
  effect as instrument. Primary.
- **Audio Mostly** — strong fit for the learnability/practice angle.
- **DAFx / SMC** — if the systems half is developed enough to lead: RT-safety
  proof + the small-language interpreter/bytecode-VM cost characterised against a
  Faust dynamic-mode baseline on the same effects (positioning, not a speed
  claim). Secondary / follow-up.
- (Note: *Leonardo Music Journal* ceased regular publication in 2019; consider
  *Organised Sound* or *Computer Music Journal* for a journal version.)

## 8. References (to verify before submission)

- N. Collins, A. McLean, J. Rohrhuber, A. Ward. *Live coding in laptop
  performance.* Organised Sound, 2003.
- TOPLAP — manifesto / draft (toplap.org).
- Y. Orlarey, D. Fober, S. Letz. *FAUST: an efficient functional approach to DSP
  programming.* (and faust.grame.fr)
- *faust2clap* (2026) — static and dynamic compilation modes; dynamic mode allows
  in-host DSP modification via runtime interpretation, preserving parameter
  identity and automation across reloads. **[full citation to confirm]** — central
  to §1/§3: hot reload in a host is no longer distinctive.
- Cmajor. Language/runtime for DSP in plugins/DAWs (cmajor.dev) — the heaviest
  feature overlap; broader and more powerful, with a separate
  project/editor/export workflow. (Successor framing of ROLI *SOUL*.)
- Cycling '74. *Gen / gen~* documentation — sample-level authoring inside
  Max / Max for Live (assumes Max literacy).
- G. Wang, P. Cook. *ChucK: a concurrent, on-the-fly audio programming
  language.* ICMC, 2003 — cited as an ancestor for on-the-fly musical coding, not
  as a plugin-authoring tool.
- S. Aaron, A. Blackwell. *From Sonic Pi to Overtone: creative musical
  experiences with domain-specific and functional languages.* 2013.
- J. McCartney. *Rethinking the computer music language: SuperCollider.* CMJ,
  2002.
- V. Zappi, A. McPherson. *Dimensionality and appropriation in digital musical
  instrument design.* NIME, 2014.
- T. Magnusson. *Sonic Writing: Technologies of Material, Symbolic, and Signal
  Inscriptions.* Bloomsbury, 2019.
- A. Blackwell. *First steps in programming: a rationale for attention investment
  models.* (end-user programming background.)

## Appendix — original platform notes (retained)

- **JUCE** was chosen for straightforward multi-format plugin generation (VST3,
  AU, Standalone) from one codebase, and its mature real-time-safety guidance
  (no audio-thread allocation/locks/blocking). VST3 SDK licensing/trademark
  terms should be reviewed at release time.
- References: juce.com/juce/features, JUCE plugin tutorial, Steinberg VST3
  developer portal (licensing).
