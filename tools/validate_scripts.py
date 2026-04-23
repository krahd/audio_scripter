#!/usr/bin/env python3
"""Basic repository-level script quality checks for .ascr files."""

from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EXAMPLES = ROOT / "examples"

ALLOWED_FUNCTIONS = {
    "sin", "cos", "tan", "abs", "sqrt", "exp", "log", "tanh", "pow",
    "min", "max", "clamp", "clip", "mix", "wrap", "fold", "crush", "smoothstep", "noise", "lpf1", "slew",
}

FUNCTION_RE = re.compile(r"\b([A-Za-z_][A-Za-z0-9_]*)\s*\(")


def lint_file(path: Path) -> list[str]:
    errors: list[str] = []
    lines = path.read_text(encoding="utf-8").splitlines()

    statements = 0
    for idx, raw in enumerate(lines, start=1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue

        statements += 1

        if not line.endswith(";"):
            errors.append(f"{path.name}:{idx}: statement must end with ';'")

        for match in FUNCTION_RE.finditer(line):
            fn = match.group(1)
            if fn not in ALLOWED_FUNCTIONS and fn not in {"if", "for", "while"}:
                # Unknown calls are likely typos.
                errors.append(f"{path.name}:{idx}: unknown function '{fn}'")

    if statements == 0:
        errors.append(f"{path.name}: script contains no statements")

    if statements > 256:
        errors.append(f"{path.name}: script has {statements} statements (max 256)")

    return errors


def main() -> int:
    scripts = sorted(EXAMPLES.glob("*.ascr"))
    if not scripts:
        print("No scripts found in examples/")
        return 1

    all_errors: list[str] = []
    for script in scripts:
        all_errors.extend(lint_file(script))

    if all_errors:
        print("Script validation failed:")
        for e in all_errors:
            print(f" - {e}")
        return 1

    print(f"Validated {len(scripts)} example scripts successfully.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
