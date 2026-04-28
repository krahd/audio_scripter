# audio_scripter language specification (v1.x summary)

For the up-to-date language/runtime behavior, see `docs/LANGUAGE_SPEC_v2.md`.

## Core model

- Per-sample script execution.
- Assignments, blocks, `if/else`, `while`, dual-form `for`, user functions, `return`, `break`, and `continue`.
- Arithmetic expressions (`+ - * /`, unary `-`), variables, and function calls.
- Persistent state via `state_*` variables.

## Built-ins (current)

- Math: `sin`, `cos`, `tan`, `abs`, `sqrt`, `exp`, `log`, `tanh`, `pow`, `min`, `max`
- Utility: `clamp`/`clip`, `mix`, `wrap`, `smoothstep`, `noise`
- DSP: `fold`, `crush`, `pulse`, `lpf1`, `hp1`, `bp1`, `svf`, `slew`, `env`, `delay`, `sat`
- Control: `gt`, `lt`, `ge`, `le`, `select`

## Constraints

- Max script statements: 256.
- Empty scripts are rejected.
- Runtime guardrails enforce instruction, loop-depth, and recursion limits.
