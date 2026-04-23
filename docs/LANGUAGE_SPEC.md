# audio_scripter language specification (v1.0)

## Grammar

- Program = zero or more statements
- Statement = `identifier '=' expression ';'`
- Expressions support precedence: unary, `* /`, then `+ -`
- Function call: `identifier '(' [expression (',' expression)*] ')'`

## Program constraints

- Maximum 256 statements per script.
- Empty scripts are rejected.

## Runtime semantics

- Script executes top-to-bottom once per sample.
- Defaults at sample start:
  - `outL = inL`
  - `outR = inR`
- Read-only values:
  - `inL`, `inR`, `sr`, `t`, `p1..p8`
- Variables prefixed `state_` persist across samples.

## Operators

- `+`, `-`, `*`, `/`
- unary `-`
- parentheses

## Functions

### Math
- `sin(x)`, `cos(x)`, `tan(x)`, `abs(x)`, `sqrt(x)`, `exp(x)`, `log(x)`, `tanh(x)`
- `pow(a, b)`, `min(a, b)`, `max(a, b)`

### Utility
- `clamp(x, lo, hi)`
- `clip(x, lo, hi)`
- `mix(a, b, amount)`
- `wrap(x, lo, hi)`

### DSP/creative
- `fold(x, lo, hi)`
- `crush(x, steps)`
- `smoothstep(edge0, edge1, x)`
- `noise(seed)`
- `lpf1(x, coeff [, id])`
- `slew(target, speed [, id])`
 - `noise(seed)`
 - `gt(a, b)`, `lt(a, b)`, `ge(a, b)`, `le(a, b)`
 - `select(cond, a, b)`
 - `pulse(freqHz, duty)`
 - `env(x, attack, release [, id])`
 - `lpf1(x, coeff [, id])`
 - `slew(target, speed [, id])`

## Error handling

- Parser errors include line numbers.
- Unknown function or wrong arity fails compile.
- Divide-by-zero evaluates to `0.0`.
