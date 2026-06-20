# Research positioning — audio_scripter

> Working notes for a paper. Draft; citations need a final pass before
> submission (see §8). Companion to [ROADMAP.md](ROADMAP.md).

## 1. Contribution (one sentence)

audio_scripter treats **writing and hot-swapping a per-sample DSP script as an
expressive musical gesture**: the performing musician authors, edits, and
instantly replaces their own audio effects *inside the DAW they already work in*,
with zero interruption to the audio stream — making effect *design* part of the
creative act rather than a separate, prior, programmer's task.

The claim is not "another way to write DSP." Mature systems already do that well
(§3). The claim is about **who** writes the effect, **when**, and **where**: the
musician, in-flow, in-place — and the language, interface, and runtime are
designed end-to-end to make that loop fast, safe, and low-floor.

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
   non-systems-programmers access to sample-rate DSP. audio_scripter narrows that
   floor deliberately (§5) and embeds the authoring loop in the host, trading
   generality for immediacy and learnability.

**Gap:** existing systems optimise for *power* (Faust, Cmajor) or for
*standalone performance* (ChucK, Sonic Pi). None positions the act of a musician
writing a signal-level effect, mid-session, with instantaneous safe hot-swap, as
the unit of expression — and evaluates it as such.

## 3. Related systems (and how we differ)

| System | Domain | Authoring locus | Per-sample DSP | Hot-swap | Floor |
| --- | --- | --- | --- | --- | --- |
| **Faust** | Functional DSP → plugin | Compile-ahead, separate toolchain | Yes | Recompile | High |
| **Max Gen~** | Patching + gen code | Inside Max patcher | Yes | Recompile-ish | Medium |
| **SOUL / Cmajor** | DSP language/runtime | Separate editor | Yes | JIT reload | Medium–high |
| **ChucK** | Strongly-timed lang | Standalone / miniAudicle | Yes | Add/replace shreds | Medium |
| **Sonic Pi** | Live-coding music | Standalone IDE | No (synth/FX graph) | Yes | Low |
| **SuperCollider** | Synthesis lang | Standalone | UGen graph | Re-eval | High |
| **audio_scripter** | **Effect scripting in-DAW** | **Inside the DAW, as a plugin** | **Yes** | **Lock-free, zero-interruption** | **Low (deliberately)** |

Our differentiators are the **locus** (a plugin in the user's existing DAW
session, not a separate environment), the **gesture** (lock-free hot-swap with no
audio dropout — the edit *is* the performance action), and a **deliberately
minimal language** tuned for a musician, not a DSP engineer.

## 4. The hot-swap as the enabler of the gesture

For "editing the effect" to be a *musical* action it must be **instantaneous and
safe**: no dropout, no glitch, no crash when a half-finished script is applied.
audio_scripter achieves this with a lock-free publish model:

- Scripts are parsed/validated **off** the audio thread; a successful compile is
  published as an immutable `shared_ptr<const CompiledProgram>` via
  `std::atomic_store`.
- The audio thread reads the current snapshot with `std::atomic_load` and runs
  it; an in-flight block always finishes against a consistent program.
- Sample-rate and state-reset are atomic *requests* consumed at block boundaries;
  runtime DSP state stays audio-thread-owned.
- Failed compiles never reach the audio thread; the previous effect keeps
  playing while the musician fixes the error.

This is the technical core that makes the artistic claim defensible, and it is
the basis of the **systems** half of the evaluation (§6.2). Note: the current
`delay()` path still allocates on first use on the audio thread — making the
RT-safety claim *provably* true is Phase 1 of the roadmap and a prerequisite for
publication.

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
- **DAFx / SMC** — if the systems half (hot-swap + bytecode VM benchmark) is
  developed enough to lead. Secondary / follow-up.
- (Note: *Leonardo Music Journal* ceased regular publication in 2019; consider
  *Organised Sound* or *Computer Music Journal* for a journal version.)

## 8. References (to verify before submission)

- N. Collins, A. McLean, J. Rohrhuber, A. Ward. *Live coding in laptop
  performance.* Organised Sound, 2003.
- TOPLAP — manifesto / draft (toplap.org).
- Y. Orlarey, D. Fober, S. Letz. *FAUST: an efficient functional approach to DSP
  programming.* (and faust.grame.fr)
- Cycling '74. *Gen / gen~* documentation.
- ROLI / Cmajor. *SOUL* and *Cmajor* language/runtime (cmajor.dev).
- G. Wang, P. Cook. *ChucK: a concurrent, on-the-fly audio programming
  language.* ICMC, 2003.
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
