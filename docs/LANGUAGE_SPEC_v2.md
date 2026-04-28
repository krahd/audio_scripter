# audio_scripter language specification (v2.2)

This document reflects the currently implemented parser/runtime behavior.

## Grammar

- Program: zero or more statements.
- Statement kinds:
  - assignment: `identifier = expression;`
  - block: `{ ... }`
  - if/else: `if (condition) statement [else statement]`
  - while: `while (condition) statement`
  - for (legacy): `for (i = start; end) statement`
  - for (extended): `for (i = start; condition; step) statement`
  - function definition: `fn name(arg1, arg2, ...) { ... }`
  - return: `return expression;`
  - break: `break;`
  - continue: `continue;`
  - expression statement: `funcCall(...);`
- Expressions support:
  - unary minus: `-x`
  - binary arithmetic: `*`, `/`, `+`, `-`
  - parentheses
  - numeric literals, variable reads, function calls

## Important semantics

- Script executes once per sample, top-to-bottom.
- At sample start runtime defaults are:
  - `outL = inL`
  - `outR = inR`
- Read-only runtime values: `inL`, `inR`, `sr`, `t`.
- Macro values: `p1..p8` (typically 0..1).
- Variables prefixed `state_` persist across samples.
- Non-`state_` variables are local to the current sample evaluation.
- Division by values near zero returns `0.0`.

## Control flow safety limits

- Max top-level statements per script: 256.
- Runtime instruction budget per sample is enforced.
- Max loop depth and recursion depth are enforced; overflow aborts execution for that sample.

## For-loop behavior

- Legacy form: `for (i = start; end)` means `i` increments by `1.0` and loop runs while `i < end`.
- Extended form: `for (i = start; condition; step)` where:
  - `condition` is re-evaluated every iteration with current `i`
  - `step` is evaluated every iteration and added to `i`
- `break;` exits the innermost loop.
- `continue;` skips to the next iteration of the innermost loop.

## Built-in functions

### Math
- `sin(x)`, `cos(x)`, `tan(x)`, `abs(x)`, `sqrt(x)`, `exp(x)`, `log(x)`, `tanh(x)`
- `pow(a, b)`, `min(a, b)`, `max(a, b)`

### Utility
- `clamp(x, lo, hi)`
- `clip(x, lo, hi)` (alias of `clamp`)
- `mix(a, b, amount)`
- `wrap(x, lo, hi)`
- `smoothstep(edge0, edge1, x)`
- `noise(seed)`

### DSP/control primitives
- `fold(x, lo, hi)`
- `crush(x, steps)`
- `pulse(freqHz, duty)`
- `lpf1(x, coeff [, id])`
- `hp1(x, coeff [, id])`
- `bp1(x, hpCoeff, lpCoeff [, id])`
- `svf(x, cutoff, q, mode [, id])` where `mode`: `0=LP`, `1=BP`, `2=HP`
- `slew(target, speed [, id])`
- `env(x, attack, release [, id])`
- `delay(x, samples [, id])`
- `sat(x, drive [, mode])` where `mode`: `0=tanh`, `1=atan`, `2=cubic`

### Comparators/selection
- `gt(a, b)`, `lt(a, b)`, `ge(a, b)`, `le(a, b)` return `1.0`/`0.0`
- `select(cond, a, b)`

## Notes for effect design

- For cleaner sound, avoid hard discontinuities when possible; combine `slew(...)` and `lpf1(...)` around stepped/gated/noise-heavy signals.
- For transient/noise effects, event-triggered bursts generally sound less static-like than always-on full-rate pseudo-noise.
- Prefer explicit wet staging (`mix`) after destructive blocks (`crush`, aggressive `sat`, deep feedback).
