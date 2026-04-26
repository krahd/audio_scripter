# audio_scripter language specification (v2.0)

## Grammar

- Program = zero or more statements
- Statement = assignment, block, if, while, for, function definition, return, expression statement
- Assignment: `identifier '=' expression ';'`
- Block: `{ ... }`
- If: `if (condition) { ... } [else { ... }]`
- While: `while (condition) { ... }`
- For: `for (init; cond; step) { ... }`
- Function definition: `fn name(args) { ... }`
- Return: `return expression;`
- Expression statement: `expression;`
- Expressions support precedence: unary, `* /`, then `+ -`
- Function call: `identifier '(' [expression (',' expression)*] ')'

## Types

- `float`, `int`, `bool` (automatic type inference for literals)
- Variables are dynamically typed, but type errors are reported at runtime

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

- `+`, `-`, `*`, `/`, `%`, `==`, `!=`, `<`, `>`, `<=`, `>=`, `&&`, `||`, `!`
- unary `-`, `!`
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
- `gt(a, b)`, `lt(a, b)`, `ge(a, b)`, `le(a, b)`
- `select(cond, a, b)`
- `pulse(freqHz, duty)`
- `env(x, attack, release [, id])`

### User-defined functions
- `fn myFunc(a, b) { ... return ...; }`
- Call with `myFunc(1, 2)`
- Supports recursion and local variables

## Error handling

- Parser errors include line numbers and token context
- Unknown function or wrong arity fails compile
- Type errors (e.g., using bool as float) are reported at runtime
- Divide-by-zero evaluates to `0.0`

## Examples

### Simple function
fn gain(x, amount) {
    return x * amount;
}
outL = gain(inL, 0.5);
outR = gain(inR, 0.5);

### If/else
if (t < 1.0) {
    outL = inL;
} else {
    outL = 0.0;
}

### While loop
n = 0;
while (n < 10) {
    n = n + 1;
}
