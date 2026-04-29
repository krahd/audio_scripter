# audio_scripter — Implementation Guide

This document serves two audiences:

- **Contributors** who want to understand the codebase, add features, or fix bugs. The implementation chapters (4–13) cover the JUCE plugin architecture, the script language pipeline, thread safety, and how to extend the engine.
- **DSP learners** who want to understand digital audio signal processing from first principles, using audio_scripter as a concrete, runnable system. Chapter 3 and chapter 15 cover the mathematics at a level suitable for undergraduate university study.

No prior JUCE knowledge is assumed, though familiarity with C++ and basic calculus is expected.

---

## Table of contents

1. [Project overview](#1-project-overview)
2. [Repository layout](#2-repository-layout)
3. [DSP fundamentals](#3-dsp-fundamentals)
4. [Plugin architecture (JUCE)](#4-plugin-architecture-juce)
5. [Script engine](#5-script-engine)
6. [Parser and AST](#6-parser-and-ast)
7. [Evaluator and execution context](#7-evaluator-and-execution-context)
8. [Built-in DSP primitives](#8-built-in-dsp-primitives)
9. [Lock-free hot-swap](#9-lock-free-hot-swap)
10. [Plugin editor (UI)](#10-plugin-editor-ui)
11. [Build system](#11-build-system)
12. [Testing](#12-testing)
13. [Adding new features](#13-adding-new-features)
14. [Contributing](#14-contributing)
15. [Advanced DSP theory](#15-advanced-dsp-theory)

---

## 1. Project overview

audio_scripter is a JUCE-based audio effect plugin (VST3, AU, Standalone) that lets
users write small DSP programs — *scripts* — that run once per audio sample. The key
design constraints are:

- **No compilation step.** Scripts are interpreted, not JIT-compiled. Users press
  Apply and the new effect is live within the current audio block.
- **Zero-copy hot-swap.** The running script is replaced atomically using a lock-free
  shared pointer. The audio thread never blocks.
- **Low overhead per sample.** The interpreter is a simple tree-walking evaluator.
  For typical scripts (10–50 AST nodes per sample) this is well within CPU budget.

The trade-off: interpretive overhead makes the script language unsuitable for
extremely tight inner loops (thousands of iterations per sample). For that use case,
native JUCE DSP code is the right tool.

---

## 2. Repository layout

```text
audio_scripter/
├── Source/
│   ├── PluginProcessor.h/cpp   # AudioProcessor: audio thread, script engine wrapper
│   ├── PluginEditor.h/cpp      # AudioProcessorEditor: UI (code editor, sliders…)
│   ├── ScriptEngine.h/cpp      # Compiler + evaluator entry point
│   ├── ScriptParser.h/cpp      # Tokeniser, recursive-descent parser, AST, evaluator
│   ├── ScriptCodeTokeniser.h/cpp  # JUCE syntax-colouring tokeniser for the editor
│   └── Constants.h             # Shared constants (kNumMacros = 8)
├── examples/                   # .ascr script files shipped with the plugin
├── docs/                       # GitHub Pages website source
├── tests/                      # Parser unit tests (Catch2)
├── tools/                      # validate_scripts.py
└── CMakeLists.txt
```

---

## 3. DSP fundamentals

This section teaches the concepts that underpin the script language and the built-in
primitives. Understanding them makes the engine code much easier to read.

### 3.1 Audio samples and sample rate

Digital audio is a stream of numbers called *samples*. Each sample is a floating-point
value in the range −1.0 to 1.0 representing instantaneous air pressure. A standard
44.1 kHz session delivers 44,100 samples per second per channel.

The plugin processes stereo audio: `inL`/`inR` (left and right input) and
`outL`/`outR` (output). A script that writes

```text
outL = inL * 0.5;
outR = inR * 0.5;
```

reduces volume by 6 dB — it halves the amplitude of every sample.

### 3.2 Per-sample processing and the audio callback

DAWs deliver audio in *blocks* — typically 64–2048 samples at a time. JUCE calls
`processBlock(AudioBuffer<float>& buffer, …)` once per block. The engine loops over
every sample in the block, running the script for each one:

```cpp
for (int s = 0; s < numSamples; ++s)
{
    ctx.t    = (float) sampleCounter / (float) currentSampleRate;
    ctx.inL  = buffer.getSample(0, s);
    ctx.inR  = buffer.getSample(1, s);
    ctx.outL = ctx.inL;   // default: pass-through
    ctx.outR = ctx.inR;
    // … run script …
    buffer.setSample(0, s, ctx.outL);
    buffer.setSample(1, s, ctx.outR);
    ++sampleCounter;
}
```

The variable `t` therefore increments by `1/sr` with every sample, giving scripts
a continuous time reference.

### 3.3 State and memory in DSP

Most interesting effects require *memory* — the output at sample N depends on
earlier samples. In audio_scripter, any variable whose name begins with `state_`
is stored in a persistent map between script executions. All other variables are
local to the current sample and start at 0 each time.

A phase-accumulator oscillator demonstrates this:

```text
state_phase = wrap(state_phase + 440.0 / sr, 0.0, 1.0);
outL = sin(6.2831853 * state_phase);
```

`state_phase` increments by `440/44100 ≈ 0.01` each sample, wrapping at 1.0.
One full cycle takes 44100/440 ≈ 100 samples — exactly 440 Hz.

### 3.4 One-pole IIR filters

A one-pole IIR (Infinite Impulse Response) filter is the simplest stateful filter.
Its recurrence is:

```text
y[n] = y[n-1] + α · (x[n] − y[n-1])
     = α · x[n]  +  (1 − α) · y[n-1]
```

Where α is a coefficient in (0, 1). Large α → fast response, high cutoff frequency.
Small α → slow response, low cutoff.

The 3 dB cutoff frequency is:

```text
f_c ≈ α · f_s / (2π)    (for small α)
```

or more precisely:

```text
α = 1 − exp(−2π · f_c / f_s)
```

`lpf1` implements a low-pass version; `hp1` subtracts the low-pass output from the
input (`x − y`) to obtain a high-pass response.

**Resonance** can be added by feeding the previous output back into the input before
filtering:

```text
filtL = lpf1(inL + prevOut * resonance, coeff, 0);
prevOut = filtL;
```

The DC gain of this structure is `1 / (1 − resonance)`. Keep `resonance < 0.7`
to avoid significant clipping on hot signals.

### 3.5 State-variable filter (SVF)

The SVF is a second-order filter that simultaneously produces low-pass (LP),
band-pass (BP), and high-pass (HP) outputs from the same state. Its recurrence:

```text
high[n] = x[n] − low[n-1] − Q⁻¹ · band[n-1]
band[n] = cutoff · high[n] + band[n-1]
low[n]  = cutoff · band[n] + low[n-1]
```

`Q` controls the resonance (sharpness of the peak at cutoff). The SVF is more
numerically stable than cascaded one-pole filters and is better suited to resonant
filter sweeps.

### 3.6 Delay lines and ring buffers

A delay line stores past samples and retrieves them later. The standard implementation
is a *ring buffer* (circular buffer):

```text
buffer[writePos] = input;
output = buffer[(writePos − delaySamples + bufferSize) % bufferSize];
writePos = (writePos + 1) % bufferSize;
```

A ring buffer of length N can hold any delay from 1 to N−1 samples. In
audio_scripter, each `delay()` lane has a ring buffer of 96,000 samples
(≈ 2.2 seconds at 44.1 kHz), stored in `ScriptEngine::delayBuffers`.

> **Historical note.** Before version 1.2, the delay buffer was stored in the
> general `persistentState` map as individual keyed entries. Because that map had a
> 128-entry limit, any delay longer than ~60 samples silently corrupted. The current
> implementation uses `std::vector<float>` ring buffers, allocated lazily per lane.

### 3.7 Envelope followers

An envelope follower tracks the amplitude of a signal over time. The `env()` builtin
uses an asymmetric one-pole smoother:

```cpp
coeff = (|x| > y) ? attack : release;
y += coeff * (|x| − y);
```

- **Attack** (signal rising): large coeff → fast tracking
- **Release** (signal falling): small coeff → slow decay

Typical compressor values:

- Attack: 1–10 ms → `coeff ≈ 1/(0.005 · sr)` to `1/(0.001 · sr)`
- Release: 50–500 ms → `coeff ≈ 1/(0.05 · sr)` to `1/(0.5 · sr)`

For a 50 ms release at 44100 Hz: `coeff = 1 / (0.05 × 44100) ≈ 0.00045`.

### 3.8 Waveshaping and saturation

A waveshaper applies a nonlinear function `f(x)` to each sample. Common choices:

| Shape | Expression | Character |
| --- | --- | --- |
| Hard clip | `clamp(x, −1, 1)` | Square-wave-like, harsh |
| tanh | `tanh(drive · x)` | Smooth, musical |
| atan | `(2/π) · atan(drive · x)` | Slightly brighter than tanh |
| Cubic | `x − x³/3` | Soft, adds 3rd harmonic |
| Fold | reflects x at boundaries | Rich, complex harmonics |

Drive > 1 amplifies before clipping, increasing harmonic content.

### 3.9 Modulation effects

Modulation effects use a **low-frequency oscillator** (LFO) — a periodic signal below ~20 Hz — to vary some parameter of the audio signal over time.

**Tremolo** modulates amplitude:

```text
y[n] = x[n] · (1 − depth + depth · LFO[n])
```

The LFO swings between 0 and 1; `depth` controls how deep the modulation goes. At `depth = 1` the signal is fully silenced at the LFO's trough; at `depth = 0.5` it dips to 50% amplitude.

**Ring modulation** multiplies the signal by a carrier sine wave, replacing the original frequencies with sum and difference sidebands. For a carrier at `fc` Hz modulating a signal at `fs` Hz, the output contains `fc + fs` and `fc − fs` but not `fs` or `fc` directly. The result is an unmistakable metallic, bell-like sound:

```text
freq = 100.0 + p1 * 1900.0;
mod = sin(6.2831853 * freq * t);
outL = inL * mod;
outR = inR * mod;
```

**Chorus** mixes the original signal with a copy delayed by a time that varies with an LFO (typically 5–30 ms). The beating between slightly detuned copies creates a lush, wide sound. Two instances with opposite LFO phases give stereo spread.

**Flanger** is like chorus but with much shorter delays (0.5–10 ms). The interaction of the delayed copy with the original creates a comb filter whose notches sweep up and down with the LFO — a distinctive jet-like "whoosh".

**Phaser** routes the signal through a chain of all-pass filters whose phase shift varies with an LFO. Mixing the phase-shifted output with the original causes certain frequencies to partially cancel, creating moving spectral notches. Unlike a flanger, a phaser's notches are not evenly spaced in frequency.

### 3.10 Dynamics processing

A **compressor** reduces the dynamic range of a signal. When the signal level exceeds a threshold, it is attenuated by a ratio R: every dB above the threshold is reduced to 1/R dB. For a ratio of 4:1, a signal 8 dB over the threshold is only 2 dB over the threshold after compression.

In practice:

1. Measure level using an envelope follower (`env`).
2. Convert to dB: `level_dB = 20 · log₁₀(level)`.
3. Compute the gain reduction: `reduction_dB = max(0, (level_dB − threshold) · (1 − 1/ratio))`.
4. Convert back to linear: `gain = 10^(−reduction_dB / 20)`.
5. Smooth the gain with a slew limiter or LP filter to avoid gain-change clicks.

A **limiter** is a compressor with a very high ratio (∞:1) — the signal never exceeds the threshold. A **gate** silences audio below a threshold using the inverse logic.

### 3.11 Spatial effects: delay and reverb

**Delay** creates distinct echoes by mixing the original signal with a time-shifted copy. Feedback delay loops the delayed signal back into the input, producing a trail of decaying echoes:

```text
state_fb = delay(inL + state_fb * feedback, delay_samples, 0);
```

The decay time (in samples) depends on both delay length and feedback:
`T60 ≈ −delay_samples / log₁₀(|feedback|) · 3 / sr` (time to decay by 60 dB).

**Reverb** simulates the dense reflections of an acoustic space. The classic **Schroeder reverb** (1962) uses:

1. Four parallel **feedback comb filters** (delay lines with gain), each with a prime-number delay length to avoid repetitive echoes.
2. Two **all-pass diffusers** in series to smear the temporal envelope.

Each comb filter produces a series of echoes; with four running in parallel at different lengths, their echoes interleave and gradually become indistinguishable from continuous noise — the characteristic sound of reverb. See `examples/reverb.ascr` for a working implementation.

### 3.12 Filters in depth: biquad and the z-transform

A **biquad filter** is the standard building block for second-order IIR filters. Its difference equation:

```text
y[n] = b0·x[n] + b1·x[n-1] + b2·x[n-2] − a1·y[n-1] − a2·y[n-2]
```

The coefficients `(b0, b1, b2, a1, a2)` determine the filter type (LP, HP, bandpass, notch, shelf, peak). All standard biquad types are derived from the same formula by substituting pre-computed coefficients based on `fc` (cutoff), `Q` (quality factor), and `gain` (for shelves/peaks).

The **z-transform** provides a mathematical framework. Replacing each delay `x[n-k]` with `z⁻ᵏ`, the filter's transfer function is:

```text
H(z) = (b0 + b1·z⁻¹ + b2·z⁻²) / (1 + a1·z⁻¹ + a2·z⁻²)
```

The roots of the denominator are the **poles**; roots of the numerator are the **zeros**. Poles near the unit circle in the complex plane create resonance peaks; poles on or outside the unit circle cause instability.

The frequency response is `H(e^{jω})` evaluated on the unit circle. A low-pass biquad has two poles near `e^{j·ωc}` (the cutoff angle) and two zeros at `z = −1` (DC = 0, Nyquist = 0 output). Plotting `|H(e^{jω})|` traces the familiar LP roll-off curve.

The SVF implementation in audio_scripter is a special case: it can be mapped to a biquad but its recurrence is written in a form that keeps both state variables (low and band) readily available, making it easy to blend between filter modes.

---

## 4. Plugin architecture (JUCE)

JUCE splits a plugin into two objects with distinct responsibilities:

### 4.1 AudioProcessor (`PluginProcessor`)

Lives on the **audio thread**. Must be real-time safe: no memory allocation, no locks,
no blocking calls. Key responsibilities:

- Owns the `ScriptEngine`.
- Receives `processBlock()` calls from the host.
- Manages the 8 macro parameters via `AudioProcessorValueTreeState` (APVTS).
- Calls `ScriptEngine::processBlock()` to run the script on every block.

```cpp
void AudioScripterAudioProcessor::processBlock(AudioBuffer<float>& buffer, …)
{
    std::array<float, 8> macros;
    for (int i = 0; i < 8; ++i)
        macros[i] = *parameters.getRawParameterValue(macroParamId(i));

    engine.processBlock(buffer, macros);
}
```

### 4.2 AudioProcessorEditor (`PluginEditor`)

Lives on the **message thread** (UI thread). Can allocate, block, and use JUCE
UI components freely. Key responsibilities:

- Displays the `CodeEditorComponent` backed by `ScriptCodeTokeniser`.
- Applies scripts via `processor.setScript(text)` → `engine.compileAndInstall(text)`.
- Parses `# pN = value` and `#@pN: Label` metadata lines to pre-set sliders and labels.
- Loads/saves `.ascr` files.

### 4.3 Parameter management

`AudioProcessorValueTreeState` (APVTS) manages the 8 macro knobs as proper DAW
parameters (automatable, saveable in the session, MIDI-mappable). Each parameter is
a `RangedAudioParameter` with range [0, 1].

The editor binds sliders to parameters via `SliderAttachment`, which keeps the slider
and the underlying parameter in sync automatically.

---

## 5. Script engine

`ScriptEngine` (Source/ScriptEngine.h, ScriptEngine.cpp) is the entry point for all
scripting activity.

### 5.1 Lifecycle

```text
compileAndInstall(source)
    │
    ├── ScriptParser::parse(source)
    │       → tokenise → parse → AST in Program struct
    │
    ├── registerBuiltins(FunctionRegistry)
    │       → populate builtins map with lambdas
    │
    ├── if errors → return {false, errors}  (no change to running script)
    │
    └── atomic_store(activeProgram, newProgram)
        stateResetRequested = true
```

On the next `processBlock()` call the audio thread detects `stateResetRequested`,
clears all persistent state, and begins executing the new script. The old script
program is held alive by the old `shared_ptr` until nothing references it.

### 5.2 Persistent state

Two kinds of state survive between samples:

1. **User state variables** (`state_*`): stored in `persistentState`, a
   `std::map<juce::String, float>`. The key is the variable name (e.g. `"state_phase"`).

2. **DSP primitive state** (`lpf1`, `env`, `slew`, etc.): stored in the same
   `persistentState` map under synthesised keys like `"__lpf1_0"` (function name +
   lane index). Each unique lane index gives an independent filter instance.

3. **Delay buffers**: stored in `delayBuffers`, a
   `std::unordered_map<int, std::vector<float>>`. Each lane maps to a 96,000-sample
   ring buffer. Write positions are tracked in `delayWritePositions`. These are
   separate from `persistentState` to avoid the 128-entry limit that would corrupt
   long delay lines.

All state is cleared whenever `compileAndInstall()` installs a new script
(`stateResetRequested`) and whenever the host calls `reset()` (plugin reset or
sample-rate change).

---

## 6. Parser and AST

`ScriptParser` (Source/ScriptParser.h, ScriptParser.cpp) converts source text into
an Abstract Syntax Tree (AST). The parser is a hand-written **recursive descent**
parser — no parser generator or external library is involved.

### 6.1 Tokeniser

`ScriptTokenizer` (Source/ScriptTokenizer.h/cpp) breaks source text into tokens:

| Token type | Examples |
| --- | --- |
| Number | `0.5`, `440.0`, `6.2831853` |
| Identifier | `inL`, `state_phase`, `fn`, `if`, `return` |
| Arithmetic operator | `+`, `-`, `*`, `/` |
| Assignment / equality | `=`, `==` |
| Comparison | `<`, `<=`, `>`, `>=`, `!=` |
| Logical | `!`, `&&`, `\|\|` |
| Bitwise | `&`, `\|`, `^`, `<<`, `>>` |
| Punctuation | `;`, `,`, `{`, `}`, `(`, `)` |
| Comment | everything after `#` on a line |

The tokeniser is also used by `ScriptCodeTokeniser` to drive JUCE's
`CodeEditorComponent` syntax highlighting.

### 6.2 Grammar (informal)

```text
program     := statement*
statement   := assignment | if_stmt | while_stmt | for_stmt
             | fn_def | return_stmt | break_stmt | continue_stmt
assignment  := identifier '=' expr ';'
if_stmt     := 'if' '(' expr ')' block ( 'else' block )?
while_stmt  := 'while' '(' expr ')' block
for_stmt    := 'for' '(' identifier '=' expr ';' expr ')' block          # legacy
             | 'for' '(' identifier '=' expr ';' expr ';' expr ')' block  # extended
fn_def      := 'fn' identifier '(' params ')' block
return_stmt := 'return' expr ';'
break_stmt  := 'break' ';'
continue_stmt := 'continue' ';'
expr        := logical_or
logical_or  := logical_and ( '||' logical_and )*
logical_and := bitwise_or  ( '&&' bitwise_or  )*
bitwise_or  := bitwise_xor ( '|'  bitwise_xor )*
bitwise_xor := bitwise_and ( '^'  bitwise_and )*
bitwise_and := equality    ( '&'  equality    )*
equality    := comparison  ( ('=='|'!=') comparison )*
comparison  := shift       ( ('<'|'<='|'>'|'>=') shift )*
shift       := add_sub     ( ('<<'|'>>') add_sub )*
add_sub     := mul_div     ( ('+'|'-') mul_div )*
mul_div     := unary       ( ('*'|'/') unary   )*
unary       := ('-'|'!') unary | primary
primary     := number | identifier | fn_call | '(' expr ')'
fn_call     := identifier '(' (expr (',' expr)*)? ')'
block       := '{' statement* '}'
```

The precedence hierarchy follows C: `||` binds loosest, unary operators bind tightest.
`&&` and `||` short-circuit (right-hand side is skipped when the result is determined
by the left). Comparison functions `gt`, `lt`, `ge`, `le`, `select` remain available
as builtins; they cover `>=` and `<=` which have no infix form.

### 6.3 AST node types

Each node inherits from either `Expr` (produces a float value) or `Statement`
(has a side effect). Key types:

| Node | Kind | Purpose |
| --- | --- | --- |
| `LiteralExpr` | Expr | Numeric constant |
| `VariableExpr` | Expr | Read a variable |
| `BinaryOpExpr` | Expr | `+`, `-`, `*`, `/`, `<`, `<=`, `>`, `>=`, `==`, `!=`, `&&`, `\|\|`, `&`, `\|`, `^`, `<<`, `>>` |
| `UnaryMinusExpr` | Expr | Arithmetic negation (`-x`) |
| `UnaryNotExpr` | Expr | Logical NOT (`!x`) |
| `FunctionCallExpr` | Expr | Builtin or user-defined call |
| `AssignmentStatement` | Statement | Write a variable |
| `IfStatement` | Statement | Conditional |
| `WhileStatement` | Statement | Loop |
| `ForStatement` | Statement | For loop (both forms) |
| `FunctionDefStatement` | Statement | Define a user function |
| `ReturnStatement` | Statement | Return from function |
| `BreakStatement` | Statement | Break from loop |
| `ContinueStatement` | Statement | Continue to next iteration |

---

## 7. Evaluator and execution context

### 7.1 EvalContext

`EvalContext` (ScriptParser.h) is a struct that carries all the mutable state needed
during one sample's execution:

```cpp
struct EvalContext
{
    float inL, inR;          // current input samples
    float outL, outR;        // output (pre-set to inL/inR each sample)
    float sr;                // sample rate
    float t;                 // elapsed time in seconds

    std::array<float,8>* macros;                        // p1–p8 knob values
    std::map<juce::String, float> locals;               // per-sample local vars
    std::map<juce::String, float>* persistentState;     // state_ vars + DSP state
    std::unordered_map<int, std::vector<float>>* delayBuffers;   // ring buffers
    std::unordered_map<int, int>* delayWritePositions;
    const FunctionRegistry* functionRegistry;           // builtin + user functions

    // Execution limits and flags
    int instructionCount, maxInstructions;  // guard against infinite loops
    int loopDepth, maxLoopDepth;
    int recursionDepth, maxRecursionDepth;
    bool executionAborted, returnTriggered;
    bool breakTriggered, continueTriggered;
    float returnValue;
};
```

`locals` is cleared at the start of each sample. `persistentState` persists forever
(until script change or reset). The `delayBuffers` live outside `persistentState` so
their size is not limited.

### 7.2 Variable resolution order

`EvalContext::getValue(name)` tries, in order:

1. `inL`, `inR`, `outL`, `outR`, `sr`, `t` — built-in read-only context variables
2. `p1`–`p8` — macro knob values (read from `*macros`)
3. `true`, `false` — boolean literals
4. `locals` map — function parameters and local assignments
5. `persistentState` map — `state_` prefixed variables

Writing (`setValue`) updates `locals` if the name is already present there (function
scope), otherwise updates `persistentState` for `state_*` names, or `locals`
for everything else.

### 7.3 Function call scoping

User-defined functions share the same `locals` map as their caller (outer variables
are visible inside functions). When a function is called:

1. The current `locals` map is saved.
2. Function parameters are written into `locals`.
3. The function body executes.
4. The saved `locals` is restored.

This means outer-scope variables that are not shadowed by a parameter name remain
readable inside the function. `p1`–`p8` are always accessible because they come from
the shared `macros` pointer, not `locals`.

### 7.4 Safety limits

To prevent runaway scripts from stalling the audio thread:

| Limit | Default | Purpose |
| --- | --- | --- |
| `maxInstructions` | 4096 | Abort if too many nodes evaluated per sample |
| `maxLoopDepth` | 1024 | Prevent deeply nested loops |
| `maxRecursionDepth` | 64 | Prevent infinite recursion |
| `kMaxPersistentStateEntries` | 128 | Cap the `persistentState` map size |

If `maxInstructions` is exceeded, `executionAborted` is set and the current sample
outputs silence (the pre-set `outL = inL`, `outR = inR` fallback is already gone by
then if the script wrote to `outL`/`outR` — in practice scripts that hit this limit
output the last partial result). The output panel shows an error on the next UI tick.

---

## 8. Built-in DSP primitives

All builtins are registered in `registerBuiltins(FunctionRegistry&)` in
ScriptEngine.cpp as lambdas stored in `registry.builtins`. Each lambda signature is:

```cpp
[](EvalContext& ctx, const std::vector<float>& args) -> float { … }
```

### 8.1 Stateless builtins

`sin`, `cos`, `tan`, `abs`, `sqrt`, `exp`, `log`, `tanh`, `pow`, `min`, `max`,
`clamp`, `clip`, `mix`, `wrap`, `fold`, `crush`, `smoothstep`, `noise`,
`gt`, `lt`, `ge`, `le`, `select`, `sat` — these read only from `args`, write nothing
to state, and are trivially thread-safe.

### 8.2 Stateful builtins (lane-based)

`lpf1`, `hp1`, `bp1`, `svf`, `slew`, `env` store a float per lane in
`persistentState` using keys generated by `stateKey("lpf1", lane)` → `"__lpf1_0"`.

The **lane** parameter lets scripts run multiple independent instances of the same
filter. Typical usage: lane 0 for left channel, lane 1 for right.

Adding a new stateful builtin follows the same pattern:

```cpp
registry.builtins["myFilter"] = [] (EvalContext& ctx, const std::vector<float>& a)
{
    const auto x    = getArg(a, 0);
    const auto coef = getArg(a, 1);
    const auto lane = laneFromArgs(a, 2);

    auto y = readLaneState(ctx, "myFilter", lane);
    y = /* … filter recurrence … */;
    writeLaneState(ctx, "myFilter", lane, y);
    return y;
};
```

### 8.3 Delay builtin

`delay(x, samples, lane)` uses a proper ring buffer:

```cpp
auto& buf = (*ctx.delayBuffers)[lane];
if ((int)buf.size() < kMaxDelaySamples)
    buf.assign(kMaxDelaySamples, 0.0f);   // lazy allocation

auto& wp = (*ctx.delayWritePositions)[lane];
int rp = wp - samples;
if (rp < 0) rp += kMaxDelaySamples;

float y = buf[rp];
buf[wp]  = x;
wp = (wp + 1) % kMaxDelaySamples;
return y;
```

Maximum delay is 96,000 samples (≈ 2.2 s at 44.1 kHz). Each lane allocates
384 KB on first use; up to ~16 independent delay lines are practical before memory
pressure becomes significant.

### 8.4 `pulse(hz, duty)`

Generates a square wave using the shared `ctx.t` time reference:

```cpp
float phase = std::fmod(ctx.t * freq, 1.0f);
return phase < duty ? 1.0f : 0.0f;
```

This is phase-coherent across calls within the same sample, and across script
reloads (because `t` resets to 0 on script install).

---

## 9. Lock-free hot-swap

The running script is stored as:

```cpp
std::shared_ptr<const CompiledProgram> activeProgram;  // in ScriptEngine
```

Replacing it is done with:

```cpp
std::atomic_store(&activeProgram, newProgram);   // UI thread
stateResetRequested.store(true);
```

Reading it on the audio thread:

```cpp
auto program = std::atomic_load(&activeProgram); // audio thread
```

`atomic_store` / `atomic_load` on `shared_ptr` (the free-function variants) are the
portable C++14 mechanism for lock-free reference-counted pointers. On most platforms
these reduce to a single CAS instruction on the internal pointer plus a ref-count
update.

If compilation fails, `compileAndInstall()` returns early without calling
`atomic_store`, so the old script continues running.

State reset is deferred to the next `processBlock()` call (audio thread) to avoid
touching `persistentState` from the UI thread.

---

## 10. Plugin editor (UI)

### 10.1 Code editor

`scriptEditor` is a `juce::CodeEditorComponent` backed by a `juce::CodeDocument`.
`ScriptCodeTokeniser` implements `juce::CodeTokeniser`, which JUCE calls for each
visible line to assign token types (keyword, builtin, number, comment, etc.). The
colour scheme is applied via `setColourScheme()`.

### 10.2 Macro sliders

The 8 `juce::Slider` instances are bound to APVTS parameters via
`juce::AudioProcessorValueTreeState::SliderAttachment`. This means:

- Moving a slider updates the DAW parameter immediately.
- The DAW can automate the parameter and the slider tracks it automatically.
- Session save/restore is handled by APVTS.

### 10.3 Script metadata

When a script is loaded or Apply is pressed, `applyScriptMetadata()` parses two kinds
of comment annotations:

```text
# p1 = 0.65        → set slider p1 to 0.65 (calls parameter->setValueNotifyingHost)
#@p1: Drive        → set the label above slider p1 to "Drive"
```

This is implemented with `std::regex` in the static helper functions
`applyMacroInitialValuesFromText` and `parseMacroLabelsFromText`.

### 10.4 Defaults button

The **Defaults** button calls `applyScriptMetadata()` without compiling the script,
letting users reset knobs to the script-specified defaults without re-applying the
effect.

---

## 11. Build system

### 11.1 CMake + JUCE FetchContent

The project uses JUCE's recommended CMake integration. JUCE is downloaded from GitHub
at configure time unless a local checkout is provided via `--juce-path`:

```cmake
FetchContent_Declare(JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG        8.0.4)
FetchContent_MakeAvailable(JUCE)
```

The plugin target is created with `juce_add_plugin(audio_scripter …)`.

`EXAMPLES_DIR` is passed to the plugin as a compile definition so the editor can
load `.ascr` files from the source tree at runtime (developer builds) while release
builds fall back to embedded examples.

### 11.2 Quick build

```bash
# Debug build (fastest iteration)
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build

# Release build with packaging
./scripts/build_release.sh --config Release --package
```

### 11.3 CI/CD

`.github/workflows/ci.yml` runs on every push and PR:

1. Installs CMake and a system JUCE dependency (Linux).
2. Runs the script validator (`python3 tools/validate_scripts.py`).
3. Builds and runs `audio_scripter_parser_tests` via CTest.

`.github/workflows/release.yml` triggers on `v*` tags, builds macOS and Windows
release artifacts, and publishes a GitHub Release.

---

## 12. Testing

### 12.1 Parser unit tests

`tests/` contains Catch2 unit tests for the parser and evaluator. Build and run with:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target audio_scripter_parser_tests
ctest --test-dir build --output-on-failure
```

Tests cover: literal parsing, arithmetic, variable assignment, function calls,
control flow, `state_` persistence, builtin functions, and error cases.

### 12.2 Script validator

`tools/validate_scripts.py` parses every `.ascr` file in `examples/` against the
same grammar the engine uses. It catches syntax errors before they reach users:

```bash
python3 tools/validate_scripts.py
```

This runs as part of CI and is the recommended first check before committing new
example scripts.

---

## 13. Adding new features

### 13.1 Adding a stateless builtin

In `ScriptEngine.cpp`, inside `registerBuiltins()`:

```cpp
registry.builtins["myFunc"] = [] (EvalContext&, const std::vector<float>& a)
{
    const auto x = getArg(a, 0);
    return std::sin(x) * std::cos(x);   // or whatever
};
```

The function is immediately available to scripts. No parser changes needed.

### 13.2 Adding a stateful (lane-based) builtin

```cpp
registry.builtins["allpass"] = [] (EvalContext& ctx, const std::vector<float>& a)
{
    const auto x    = getArg(a, 0);
    const auto coef = clampf(getArg(a, 1), 0.0f, 0.99f);
    const auto lane = laneFromArgs(a, 2);

    auto s = readLaneState(ctx, "allpass", lane);
    const float y = -coef * x + s + coef * /* new state */;
    // All-pass: y = -g*x + s;  s_new = x + g*y
    const float sNew = x + coef * y;
    writeLaneState(ctx, "allpass", lane, sNew);
    return y;
};
```

No header changes required — state lives in the existing `persistentState` map.

### 13.3 Adding a new keyword

1. Add the token type to the `TokenType` enum in `ScriptTokenizer.h`.
2. Recognise it in `ScriptTokenizer::getNextToken()`.
3. Create an AST node struct that inherits from `Statement` or `Expr`.
4. Add a `parse<Keyword>` method to `ScriptParser` that constructs the new node.
5. Call it from `ScriptParser::parseStatement()`.
6. Implement `execute()`/`evaluate()` on the new node.
7. Add it to `ScriptCodeTokeniser` so it gets syntax-highlighted.

### 13.4 How infix operators were added (`<`, `>`, `==`, `!=`, `!`, `&&`, `||`, `&`, `|`, `^`, `<<`, `>>`)

These operators were added in v0.0.8. The implementation involved three layers:

1. **Tokeniser** (`ScriptTokenizer.h/cpp`): New `TokenType` enum values for each operator (`less`, `lessEqual`, `greater`, `greaterEqual`, `equalEqual`, `notEqual`, `notOp`, `andAnd`, `orOr`, `ampersand`, `pipe`, `caret`, `shiftLeft`, `shiftRight`). Single-character operators use one-character lookahead to disambiguate: `=` vs `==`, `<` vs `<<` vs `<=`, `>` vs `>>` vs `>=`, `!` vs `!=`, `&` vs `&&`, `|` vs `||`.

2. **Parser** (`ScriptParser.h/cpp`): Eight new parse methods inserted between `parseExpression()` and `parseMulDiv()` to implement the C-like precedence hierarchy. `&&` and `||` are handled with explicit short-circuit logic — the right operand expression is only evaluated when the left operand does not determine the result.

3. **Evaluator** (`ScriptParser.cpp`, `BinaryExpr::evaluate`): New `Op` enum cases map to the corresponding float operations. Bitwise operators cast to `int32_t`, operate, and cast back to `float`. Comparisons return `1.0f` or `0.0f`.

The syntax highlighter (`ScriptCodeTokeniser.cpp`) was also updated to colour the new operator characters as `operatorToken`.

### 13.5 Adding a UI element

1. Declare the component in `PluginEditor.h`.
2. Initialise, add listener, and call `addAndMakeVisible()` in the constructor.
3. Remove the listener in the destructor.
4. Position it in `resized()`.
5. Handle the event in `buttonClicked()`, `sliderValueChanged()`, etc.

---

## 14. Contributing

- Open issues and PRs against `main`.
- CI runs `validate_scripts.py` and the parser tests automatically; make sure both
  pass before requesting a review.
- Follow the existing code style: four-space indentation, JUCE naming conventions,
  no external dependencies beyond JUCE and the C++ standard library.
- New builtin functions should be documented in `Source/ScriptEngine.cpp`'s
  `helpText()` and in `docs/LANGUAGE_SPEC.md`.
- New example scripts must pass `validate_scripts.py` and should include `#@pN:`
  labels and `# pN = value` defaults for all used macro parameters.

---

---

## 15. Advanced DSP theory

This chapter goes deeper into the mathematics underlying the built-in primitives and common audio effect algorithms. It is written to serve as independent study material; each subsection includes both theoretical derivation and a concrete audio_scripter script.

### 15.1 The Discrete Fourier Transform

The **Discrete Fourier Transform** (DFT) expresses a block of N samples as a sum of N complex sinusoids:

```text
X[k] = Σ_{n=0}^{N-1}  x[n] · e^{−j2πkn/N}
```

`X[k]` is a complex number. Its magnitude `|X[k]|` is the amplitude of the frequency bin at `k · fs / N` Hz; its argument is the phase.

The **inverse DFT** reconstructs the signal:

```text
x[n] = (1/N) · Σ_{k=0}^{N-1}  X[k] · e^{j2πkn/N}
```

The DFT is computed efficiently by the **Fast Fourier Transform** (FFT), which reduces the O(N²) DFT to O(N log N) by exploiting the periodicity of the twiddle factors `e^{j2πkn/N}`.

**Practical application**: audio_scripter does not expose an FFT (per-sample processing is inherently time-domain), but understanding the DFT is essential for predicting what a filter will do. The frequency response of any LTI system at frequency `f` is the DFT of its impulse response evaluated at bin `k = f · N / fs`.

**Parseval's theorem**: the total power is preserved between domains:

```text
Σ |x[n]|² = (1/N) · Σ |X[k]|²
```

This means an LP filter that attenuates the upper half of the spectrum reduces total signal power.

### 15.2 Linear time-invariant systems and convolution

A system is **linear** if it obeys superposition: `f(ax + by) = a·f(x) + b·f(y)`. It is **time-invariant** if delaying the input delays the output by the same amount. Nearly all useful filters are LTI.

An LTI system is completely characterised by its **impulse response** `h[n]` — the output when the input is a single unit impulse `δ[0] = 1, δ[n≠0] = 0`. The output for any input is the **convolution**:

```text
y[n] = Σ_{k=0}^{∞}  h[k] · x[n−k]
```

For a **FIR** (finite impulse response) filter, `h[k] = 0` for `k > M`, so the sum has M+1 terms. FIR filters are always stable (no feedback), have linear phase (optional), but require many taps for sharp cutoffs.

For an **IIR** filter, `h[k]` is nonzero for all `k ≥ 0` (infinite), meaning the output depends on all past inputs. The one-pole LP filter has `h[k] = α · (1−α)^k` — an exponentially decaying impulse response. IIR filters achieve sharp cutoffs with fewer coefficients than FIR but can be unstable.

**Short FIR in audio_scripter** — a 4-tap moving average:

```text
# Average 4 consecutive samples → low-pass effect
y = (inL + delay(inL, 1.0, 0) + delay(inL, 2.0, 1) + delay(inL, 3.0, 2)) * 0.25;
outL = y;
outR = (inR + delay(inR, 1.0, 3) + delay(inR, 2.0, 4) + delay(inR, 3.0, 5)) * 0.25;
```

Its frequency response is a sinc-like function with the first null at `fs / 4`. This removes high frequencies but also introduces ripple; a Hamming-windowed FIR with the same cutoff would be smoother.

### 15.3 Harmonic distortion and waveshaping

When a sine wave at frequency `f` passes through a nonlinearity `f(x)`, the output contains harmonics at `2f, 3f, 4f, ...`. The amplitudes of these harmonics depend on the Taylor coefficients of `f` around zero.

For `tanh(d·x)`:

```text
tanh(d·x) = d·x − (d·x)³/3 + 2(d·x)⁵/15 − ...
```

Only odd powers appear, so `tanh` generates only **odd harmonics** (3rd, 5th, ...). This produces a sound analogous to push-pull transistor distortion.

For `|x|` (full-wave rectification, an asymmetric waveshaper):

```text
|x| = 2/π + (4/π) · Σ_{k=1}^{∞} (-1)^(k+1) / (4k²-1) · cos(2k·2πf·t)
```

This generates only **even harmonics** (2nd, 4th, ...) — the character of tape saturation and tube amplifiers.

**Total Harmonic Distortion** (THD) measures distortion:

```text
THD = sqrt(P2 + P3 + P4 + ...) / P1
```

where `P_k` is the power at the k-th harmonic. Typical amplifier targets: < 0.01% THD at rated power. Guitar distortion pedals intentionally operate at > 10% THD.

**Wavefolding** is a more extreme case. When the input exceeds the boundary `hi`, it folds back symmetrically, then folds again if it exceeds `lo` on the return, and so on. Each fold adds more harmonic content. For a heavily driven sine wave the output spectrum becomes rich and inharmonic, producing metallic timbres characteristic of FM synthesis and modular synthesis.

### 15.4 Oscillator theory and phase distortion

Every waveform can be generated via a **phase accumulator**: a state variable that increases by `f/sr` each sample and wraps at 1.0. The accumulated phase is then mapped to a waveform shape.

```text
state_phase = wrap(state_phase + freq / sr, 0.0, 1.0);
outL = shape(state_phase);   # substitute any shaping function
```

Common shapes:

| Waveform | Expression | Spectrum |
| --- | --- | --- |
| Sine | `sin(2π · phase)` | Fundamental only |
| Sawtooth | `2·phase − 1` | All harmonics, 1/n amplitude |
| Square | `phase < 0.5 ? 1.0 : -1.0` | Odd harmonics, 1/n amplitude |
| Triangle | `abs(4·phase − 2) − 1` | Odd harmonics, 1/n² amplitude |

A **sawtooth** has energy at all harmonics falling off as `1/n`. Its spectrum has a "bright" character. A **triangle** has the same harmonic structure but amplitude falls off as `1/n²` — much darker.

**Aliasing**: the digital sawtooth (naïve phase accumulator with linear ramp) produces aliasing — spectral copies folded back from frequencies above Nyquist. Correct oscillators use **band-limited synthesis** (e.g. polyBLEP or summing Fourier series). The simple waveforms in audio_scripter scripts alias at high pitches; this is often acceptable for effects but unsuitable for pitched instruments.

**Phase distortion synthesis** (used in Casio CZ synthesisers) warps the phase before mapping to a waveform. For example, squeezing the first half of the phase makes the sine wave spend more time near the peak, creating a more aggressive timbre without changing pitch.

### 15.5 Feedback systems and stability

Any script that feeds the output (or delayed output) back into the input is a **feedback system**. The critical stability condition for a linear feedback loop is the **Nyquist criterion**: the loop gain must be less than 1 at all frequencies where the phase shift is exactly 360°.

For the simplest case — a delay line with feedback:

```text
state_fb = delay(inL + state_fb * fb, d, 0);
```

The loop gain is `|fb|` at all frequencies. Stability requires `|fb| < 1`. As `fb → 1`, the decay time approaches infinity — infinite reverb tail. `|fb| > 1` causes exponential growth and clipping.

For a filter followed by feedback, the frequency-dependent phase shift means some frequencies may become unstable before others. The SVF manages this: the state variables inherently bound the response at all frequencies for `q > 0.5` and `cutoff < 0.99`.

**Self-oscillation**: when a filter's resonance (feedback gain) reaches 1, the filter oscillates at its cutoff frequency even without an input signal. This is exploited in modular synthesis.

### 15.6 The decibel scale and psychoacoustics

Human hearing is roughly logarithmic in both frequency and amplitude. The decibel scales this:

| dB | Amplitude ratio | Perceived change |
| --- | --- | --- |
| +6 | ×2 | Roughly twice as loud |
| 0 | 1 | Reference |
| −6 | ×0.5 | Roughly half as loud |
| −20 | ×0.1 | Much quieter |
| −60 | ×0.001 | Very faint |
| −∞ | 0 | Silence |

The **Fletcher–Munson curves** (equal-loudness contours) show that sensitivity varies with frequency. At 1 kHz, 40 dB SPL sounds as loud as 60 dB at 100 Hz. This is why bass sounds "disappear" at low listening volumes.

**Masking**: a loud signal can make nearby-frequency soft signals inaudible. This is the basis of perceptual audio codecs (MP3, AAC): sounds that would be masked are discarded to save bits. Understanding masking helps explain why saturation harmonic content at frequencies above a loud fundamental is often imperceptible.

### 15.7 Designing multi-stage effects

Complex effects are built from simple primitives in series, parallel, or with feedback.

**Series** (signal chain): one effect feeds into another. The order matters: a compressor before a distortion pedal is very different from distortion before compression.

**Parallel** (wet/dry, layering): multiple signal paths are mixed. This is the basis of chorus, reverb send/return, and multi-band processing.

**Feedback** (recursion): a portion of the output is fed back into the input after processing. Delay → feedback loop produces echo; filter → feedback produces resonance; all-pass cascade → feedback produces reverberation.

#### Worked example: signal compander for noise reduction

A compander is a compressor on the way in and an expander on the way out. It was used in analogue tape recording to reduce tape hiss:

1. Before recording: compress the dynamic range by 2:1 (quieter parts are boosted).
2. After playback: expand by 1:2 (the boost is undone, but the constant-level tape noise is also attenuated).

In a script:

```text
# Simple compander: compress on encode, expand on decode (run twice in chain)
# Encode half: set p1 = 0.5 (compress), Decode half: set p1 = 1.5 (expand)
ratio = 0.5 + p1;       # p1 controls encode/decode
env_l = env(abs(inL), 0.005, 0.1, 0);
env_r = env(abs(inR), 0.005, 0.1, 1);
thresh = 0.1;
gainL = select(gt(env_l, thresh), pow(env_l / thresh, ratio - 1.0), 1.0);
gainR = select(gt(env_r, thresh), pow(env_r / thresh, ratio - 1.0), 1.0);
outL = inL * gainL;
outR = inR * gainR;
```

### 15.8 Practical script performance

Per-sample execution at 44100 Hz means each sample has ≈ 22 µs of budget. A modern CPU executing ~10⁹ simple operations per second has roughly 22,000 operations per sample. The interpreter overhead (AST node dispatch, map lookup) costs 10–20 "operations" per node. With 4096 instruction maximum:

- Simple effects (10 nodes): ≈ 200–400 operations → well within budget.
- Medium effects (100 nodes): ≈ 1,000–4,000 operations → comfortable at 44.1 kHz.
- Heavy scripts (500+ nodes with loops): may exceed budget and trigger abort.

Practical optimisation tips:

1. **Precompute constants once** per sample rather than re-evaluating expressions inside loops.
2. **Use the `id`-lane primitives** (`lpf1`, `svf`, etc.) instead of manual state variables for filter state — the built-in implementations avoid map lookups.
3. **Keep loops short** — a `for` loop over 12 iterations costs 12× the per-loop overhead.
4. **Avoid deeply nested function calls** — each call saves/restores the locals map.

---

*audio_scripter is released under the MIT License. © 2026 krahd.*
