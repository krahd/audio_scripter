# audio_scripter — script language manual

Version 2.x · reflects the implemented parser and runtime

---

## Table of contents

1. [Introduction](#introduction)
2. [Your first script](#your-first-script)
3. [Execution model](#execution-model)
4. [Variables](#variables)
5. [Expressions and operators](#expressions-and-operators)
6. [Control flow](#control-flow)
7. [User-defined functions](#user-defined-functions)
8. [Built-in function reference](#built-in-function-reference)
9. [Effect design patterns](#effect-design-patterns)
10. [Safety limits](#safety-limits)

---

## Introduction

The audio_scripter script language is a small, purpose-built language for **per-sample audio DSP**. Every script runs once for each audio sample in the stream. The goal is to keep things close to the maths: read the input sample, compute something, write the output — with access to smoothing filters, delay lines, envelope followers, and other DSP primitives as first-class functions.

Scripts are hot-reloaded: press **Apply** in the plugin and the new script takes effect with zero interruption to audio.

---

## Your first script

The minimal script is empty — audio passes through unchanged because `outL` and `outR` default to `inL` and `inR`.

A simple drive/saturation effect:

```text
# p1 controls drive amount
drive = 1.0 + p1 * 5.0;
outL = tanh(inL * drive);
outR = tanh(inR * drive);
```

A sine-wave ring modulator:

```text
# p1 sets modulation frequency (100–2000 Hz)
freq = 100.0 + p1 * 1900.0;
mod = sin(6.2831853 * freq * t);
outL = inL * mod;
outR = inR * mod;
```

A one-pole low-pass filter (cutoff controlled by p1):

```text
coeff = 0.002 + p1 * 0.3;
outL = lpf1(inL, coeff, 0);
outR = lpf1(inR, coeff, 1);
```

---

## Execution model

- The script runs **once per audio sample**, top to bottom.
- At the start of each sample the runtime sets default values (see [Variables](#variables)).
- The script reads `inL`/`inR`, computes, then writes `outL`/`outR`. Whatever is in `outL`/`outR` when the script ends becomes the plugin output for that sample.
- Variables not prefixed `state_` are **reset every sample** — they are scratch space for that one computation.
- Variables prefixed `state_` **persist between samples** — they are the mechanism for building filters, oscillators, delay lines, and any other memory.
- The runtime enforces per-sample instruction budgets and loop/recursion depth limits (see [Safety limits](#safety-limits)).

---

## Variables

### Read-only runtime values

| Name | Type | Description |
| --- | --- | --- |
| `inL` | float | Left input sample, nominally in −1.0 … 1.0 |
| `inR` | float | Right input sample, nominally in −1.0 … 1.0 |
| `sr` | float | Sample rate in Hz (e.g. `44100.0`) |
| `t` | float | Elapsed time in seconds, increases continuously |
| `p1`…`p8` | float | Macro knobs, always in 0.0 … 1.0, DAW-automatable |

### Output variables

| Name | Default | Description |
| --- | --- | --- |
| `outL` | `inL` | Left output sample — assign to produce output |
| `outR` | `inR` | Right output sample — assign to produce output |

### Local variables

Any identifier that does not start with `state_` is a **local** — it lives only for the current sample. Locals do not need to be declared; first assignment creates them.

```text
x = inL * 2.0;   # x is a local — gone after this sample
```

### State variables

Any variable whose name starts with `state_` **persists across samples**. This is the standard way to build stateful DSP.

```text
# Simple phase accumulator (440 Hz sine)
state_phase = wrap(state_phase + 440.0 / sr, 0.0, 1.0);
outL = sin(6.2831853 * state_phase);
outR = outL;
```

State variables are initialised to `0.0` on the first sample after the script is loaded.

### Boolean literals

`true` evaluates to `1.0`; `false` evaluates to `0.0`. All values in the language are floats — there is no separate boolean type.

---

## Expressions and operators

### Arithmetic

| Operator | Meaning |
| --- | --- |
| `a + b` | addition |
| `a - b` | subtraction |
| `-a` | unary negation |
| `a * b` | multiplication |
| `a / b` | division — returns `0.0` when the divisor is within 1e-9 of zero |

Parentheses work as expected. There is no exponentiation operator — use `pow(a, b)`.

### Comparison

There are no `<`, `>`, `==`, `!=` operators. Use the comparison functions instead:

| Function | Returns |
| --- | --- |
| `gt(a, b)` | `1.0` if `a > b`, else `0.0` |
| `lt(a, b)` | `1.0` if `a < b`, else `0.0` |
| `ge(a, b)` | `1.0` if `a >= b`, else `0.0` |
| `le(a, b)` | `1.0` if `a <= b`, else `0.0` |
| `select(c, a, b)` | `a` if `c != 0.0`, else `b` |

These return floats and can be used in any arithmetic expression. In `if` and `while` conditions, any non-zero value is truthy.

---

## Control flow

### Comments

```text
# Everything after # on a line is a comment
```

### Assignment

```text
x = expression;   # must end with a semicolon
```

### if / else

```text
if (condition) {
    ...
} else {
    ...
}
```

The `else` branch is optional. Braces are optional for single-statement bodies, but using them is recommended.

```text
# Gate: silence output if input is very quiet
amp = env(abs(inL), 0.01, 0.1, 0);
if (lt(amp, 0.01)) {
    outL = 0.0;
    outR = 0.0;
}
```

### while

```text
while (condition) {
    ...
}
```

### for — legacy form

```text
for (i = start; end) { ... }
```

`i` is initialised to `start`, incremented by `1.0` each iteration, and the loop runs while `i < end`.

```text
# Sum four harmonics
sum = 0.0;
for (i = 1.0; 5.0) {
    sum = sum + sin(6.2831853 * i * 440.0 * t);
}
outL = sum * 0.25;  outR = outL;
```

### for — extended form

```text
for (i = start; condition; step) { ... }
```

`condition` is re-evaluated with the current `i` before each iteration. `step` is evaluated and added to `i` at the end of each iteration.

```text
# Walk backwards in steps of 0.5
for (i = 10.0; gt(i, 0.0); -0.5) {
    ...
}
```

### break and continue

`break;` exits the innermost loop immediately.
`continue;` skips to the next iteration of the innermost loop.

```text
fn iterFold(x, drv, iters) {
    y = x * drv;
    for (k = 0.0; lt(k, 12.0); 1.0) {
        if (ge(k, iters)) { break; }
        if (lt(abs(y), 0.02)) { continue; }
        y = fold(y, -1.0, 1.0);
    }
    return y * 0.65;
}
```

---

## User-defined functions

```text
fn name(arg1, arg2, ...) {
    ...
    return expression;
}
```

- Functions are defined at the top level of a script.
- All arguments and local variables inside the function are scoped to the call.
- `return` exits the function with a value. A function that falls off the end returns `0.0`.
- Functions can call other user-defined functions, but recursion depth is bounded (see [Safety limits](#safety-limits)).
- User functions **cannot** read or write `state_` variables — use the `id`-lane primitives (e.g. `lpf1`, `slew`) for stateful operations inside functions.

```text
fn softClip(x, drive) {
    return tanh(x * (1.0 + drive)) / (1.0 + drive);
}

outL = softClip(inL, p1 * 4.0);
outR = softClip(inR, p1 * 4.0);
```

### Macro labels and initial values

Comments with a special syntax annotate the macro knobs shown in the plugin:

```text
# @p1: Drive
# @p2: Tone
# p1 = 0.5
# p2 = 0.3
```

`# @pN: Label` sets the label displayed on knob N.
`# pN = value` sets the knob's initial value (0.0 – 1.0) when the script is loaded.

---

## Built-in function reference

### Math

| Function | Description |
| --- | --- |
| `sin(x)` | Sine — `x` in radians (`2π ≈ 6.2831853`) |
| `cos(x)` | Cosine |
| `tan(x)` | Tangent — diverges near ±π/2 |
| `abs(x)` | Absolute value |
| `sqrt(x)` | Square root — `x` clamped to 0 internally |
| `exp(x)` | e raised to x |
| `log(x)` | Natural logarithm — `x` clamped to 1e-12 internally |
| `tanh(x)` | Hyperbolic tangent — smooth soft-clipper, output in (−1, 1) |
| `pow(a, b)` | `a` raised to the power `b` |
| `min(a, b)` | Smaller of two values |
| `max(a, b)` | Larger of two values |

### Shaping

| Function | Description |
| --- | --- |
| `clamp(x, lo, hi)` | Hard-limit `x` to [lo, hi] |
| `clip(x, lo, hi)` | Alias for `clamp` |
| `mix(a, b, t)` | Linear interpolation: `a + (b − a) * t` |
| `wrap(x, lo, hi)` | Wrap `x` cyclically inside [lo, hi] |
| `fold(x, lo, hi)` | Fold `x` back at boundaries (wavefolder) |
| `crush(x, steps)` | Quantise to N amplitude levels (bit-crusher effect) |
| `smoothstep(e0, e1, x)` | Smooth 0→1 S-curve between `e0` and `e1` |

### Noise and oscillators

| Function | Description |
| --- | --- |
| `noise(seed)` | Deterministic pseudo-random, output in (−1, 1). Use `noise(t * sr)` for per-sample white noise; use a slowly changing seed for LFO-rate randomness. |
| `pulse(hz, duty)` | Square wave: `1.0` while phase < `duty`, else `0.0`. Phase is derived from absolute time `t`. |

### Filters and smoothing

All stateful filter primitives accept an optional integer `id` argument that selects an independent internal state lane. Use different `id` values when you need the same primitive for left and right channels, or for multiple independent filter instances.

| Function | Description |
| --- | --- |
| `lpf1(x, coeff [, id])` | One-pole low-pass filter. `coeff` ≈ 0.002 is very dark; ≈ 0.5 is bright. |
| `hp1(x, coeff [, id])` | One-pole high-pass (x minus its internal lpf1). Same `coeff` interpretation as `lpf1`. |
| `bp1(x, hpCoeff, lpCoeff [, id])` | Band-pass: hp1 followed by lpf1 in series. `hpCoeff` controls the low cut; `lpCoeff` controls the high cut. |
| `svf(x, cutoff, q, mode [, id])` | State-variable filter. `cutoff` is a normalised coefficient (0.001–0.99); `q` is resonance (min 0.05, default 0.7); `mode`: 0 = low-pass, 1 = band-pass, 2 = high-pass. |
| `slew(target, speed [, id])` | Slew-rate limiter — moves toward `target` by at most `speed` units per sample. Useful for smoothing stepped or gated control signals. |
| `env(x, attack, release [, id])` | Envelope follower. Tracks `abs(x)` with asymmetric smoothing. `attack` and `release` are per-sample coefficients (0 = instant, 1 = frozen). |

**Choosing lpf1 coefficients from a cutoff frequency:**

```text
coeff = 1.0 - exp(-6.2831853 * freqHz / sr);
outL = lpf1(inL, coeff, 0);
```

### Delay

| Function | Description |
| --- | --- |
| `delay(x, samples [, id])` | Delay line. Delays `x` by `samples` samples (clamped 1–96000). Each `id` lane has its own 96000-sample buffer. |

```text
# Short delay widener
base = 12.0 + p1 * 500.0;
outL = delay(inL, base, 0);
outR = delay(inR, base * 1.3, 1);
```

### Saturation

| Function | Description |
| --- | --- |
| `sat(x, drive [, mode])` | Soft-clipper. `drive` ≥ 0 (0 = unity gain). `mode`: 0 = tanh (default), 1 = atan, 2 = cubic polynomial. |

Mode comparison:

- `mode 0` (tanh): smoothest, most natural-sounding.
- `mode 1` (atan): slightly more open top-end.
- `mode 2` (cubic): harder knee, clips at ±1.

### Comparators and selection

| Function | Returns |
| --- | --- |
| `gt(a, b)` | `1.0` if `a > b`, else `0.0` |
| `lt(a, b)` | `1.0` if `a < b`, else `0.0` |
| `ge(a, b)` | `1.0` if `a >= b`, else `0.0` |
| `le(a, b)` | `1.0` if `a <= b`, else `0.0` |
| `select(cond, a, b)` | `a` if `cond != 0.0`, else `b` |

---

## Effect design patterns

### Phase-accumulator oscillator

The standard way to generate a waveform at a specific frequency without phase drift:

```text
# 440 Hz sine
state_phase = wrap(state_phase + 440.0 / sr, 0.0, 1.0);
outL = sin(6.2831853 * state_phase);
outR = outL;
```

Replace `sin(...)` with any waveform: `pulse`, `fold`, etc.

### Envelope follower gate

```text
# @p1: Threshold
# @p2: Wet
amp = env(abs(inL), 0.001, 0.05, 0);
gate = smoothstep(p1 * 0.3, p1 * 0.3 + 0.05, amp);
outL = mix(inL * 0.05, inL, gate);
outR = mix(inR * 0.05, inR, gate);
```

### Wet/dry blending

Always expose a wet/dry control — it makes destructive effects usable in a mix:

```text
procL = sat(inL, p1 * 8.0, 0);
procR = sat(inR, p1 * 8.0, 0);
outL = mix(inL, procL, p2);
outR = mix(inR, procR, p2);
```

### Stereo widening with micro-delay

```text
# @p1: Time   @p2: Width
base = 10.0 + p1 * 600.0;
outL = delay(inL, base, 0);
outR = delay(inR, base * (1.0 + p2 * 0.9), 1);
```

### State-variable filter sweep

```text
# @p1: Cutoff  @p2: Q  @p3: Mode (LP/BP/HP)  @p4: Wet
cut = 0.005 + p1 * 0.4;
q   = 0.3 + p2 * 2.0;
md  = select(gt(p3, 0.66), 2.0, select(gt(p3, 0.33), 1.0, 0.0));
fL  = svf(inL, cut, q, md, 0);
fR  = svf(inR, cut, q, md, 1);
outL = mix(inL, fL, p4);
outR = mix(inR, fR, p4);
```

### Iterative wavefold with loop controls

```text
# @p1: Drive  @p2: Iterations  @p3: Wet
fn iterFold(x, drv, iters) {
    y = x * drv;
    for (k = 0.0; lt(k, 12.0); 1.0) {
        if (ge(k, iters)) { break; }
        if (lt(abs(y), 0.02)) { continue; }
        y = fold(y, -1.0, 1.0);
    }
    return y * 0.65;
}

drive = 1.0 + p1 * 7.0;
iters = 1.0 + p2 * 11.0;
outL = mix(inL, iterFold(inL, drive, iters), p3);
outR = mix(inR, iterFold(inR, drive, iters), p3);
```

### Avoiding clicks and noise artefacts

- Use `slew(target, speed, id)` to smooth any stepped or gated control value before multiplying it onto audio.
- Use `lpf1` on a noise source (`noise(t * sr)`) to get coloured noise rather than wideband white noise.
- Use `env` to track signal level, then `smoothstep` to create a soft threshold for gates or ducking.
- Prefer event-triggered noise bursts over always-on full-rate `noise(t * sr)` unless white noise is intentional.

---

## Safety limits

| Limit | Value |
| --- | --- |
| Maximum top-level statements per script | 256 |
| Maximum instructions per sample | 4096 |
| Maximum loop depth | enforced |
| Maximum recursion depth | enforced |
| Maximum persistent state entries | 128 |
| Maximum delay buffer per lane | 96000 samples |

When an instruction or depth limit is exceeded, execution is aborted for that sample and the output defaults to the unmodified input. The output panel in the plugin will show an error on compile; runtime budget overruns are silent (audio passes through).
