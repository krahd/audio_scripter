#!/usr/bin/env python3
"""Basic repository-level script quality checks for .ascr files."""

from __future__ import annotations

import re
from pathlib import Path
from typing import Iterable

ROOT = Path(__file__).resolve().parents[1]
EXAMPLES = ROOT / "examples"

ALLOWED_FUNCTIONS = {
    "sin", "cos", "tan", "abs", "sqrt", "exp", "log", "tanh", "pow",
    "min", "max", "clamp", "clip", "mix", "wrap", "fold", "crush", "smoothstep", "noise", "gt", "lt", "ge", "le", "select", "pulse", "env", "lpf1", "slew",
    "hp1", "bp1", "svf", "delay", "sat",
}

FUNCTION_RE = re.compile(r"\b([A-Za-z_][A-Za-z0-9_]*)\s*\(")
MACRO_RE = re.compile(r"\bp([1-8])\b")
MACRO_LABEL_RE = re.compile(r"^\s*#\s*@p([1-8])\s*[:=]\s*(.*?)\s*$", re.IGNORECASE)
MACRO_DEFAULT_RE = re.compile(r"^\s*#\s*p([1-8])\s*=\s*([-+]?(?:\d+\.?\d*|\.\d+))\s*$", re.IGNORECASE)




def lint_file(path: Path) -> list[str]:
    errors: list[str] = []
    lines = path.read_text(encoding="utf-8").splitlines()

    statements = 0
    seen_vars = set()
    user_functions = set()
    used_macros = set()
    labelled_macros = set()
    defaulted_macros = set()
    block_stack = []  # Track block depth for unreachable code
    unreachable_stack = [False]  # Track unreachable code per block

    for idx, raw in enumerate(lines, start=1):
        line = raw.strip()
        label_match = MACRO_LABEL_RE.match(raw)
        if label_match:
            if not label_match.group(2).strip():
                errors.append(f"{path.name}:{idx}: macro label cannot be empty")
            labelled_macros.add(label_match.group(1))

        default_match = MACRO_DEFAULT_RE.match(raw)
        if default_match:
            value = float(default_match.group(2))
            if value < 0.0 or value > 1.0:
                errors.append(f"{path.name}:{idx}: macro default must be in 0.0..1.0")
            defaulted_macros.add(default_match.group(1))

        if not line or line.startswith("#"):
            continue

        used_macros.update(MACRO_RE.findall(line))

        # Detect function definitions
        if line.startswith("fn "):
            fn_name = line.split()[1].split("(")[0]
            user_functions.add(fn_name)
            block_stack.append("fn")
            unreachable_stack.append(False)
            continue

        # Track block entry/exit (very basic, assumes { and } on their own lines or at block start/end)
        if line == "{" or line.endswith("{"):
            block_stack.append("block")
            unreachable_stack.append(False)
            continue
        if line == "}" or line.endswith("};"):
            if block_stack:
                block_stack.pop()
                unreachable_stack.pop()
            continue

        # Unreachable code detection (only inside function bodies)
        if unreachable_stack[-1]:
            errors.append(f"{path.name}:{idx}: unreachable code after return statement")

        statements += 1

        if not line.endswith(";"):
            errors.append(f"{path.name}:{idx}: statement must end with ';'")

        if line.startswith("return") and block_stack and block_stack[-1] == "fn":
            unreachable_stack[-1] = True

        # Track variable assignments
        if "=" in line and not line.startswith("if") and not line.startswith("while") and not line.startswith("for"):
            var = line.split("=")[0].strip()
            if var:
                seen_vars.add(var)

        # Check for unknown functions (allow user-defined)
        for match in FUNCTION_RE.finditer(line):
            fn = match.group(1)
            if fn not in ALLOWED_FUNCTIONS and fn not in user_functions and fn not in {"if", "for", "while", "return"}:
                errors.append(f"{path.name}:{idx}: unknown function '{fn}'")

    if statements == 0:
        errors.append(f"{path.name}: script contains no statements")

    if statements > 256:
        errors.append(f"{path.name}: script has {statements} statements (max 256)")

    for macro in sorted(used_macros):
        if macro not in labelled_macros:
            errors.append(f"{path.name}: p{macro} is used but has no '#@p{macro}: label' metadata")
        if macro not in defaulted_macros:
            errors.append(f"{path.name}: p{macro} is used but has no '# p{macro} = value' default metadata")

    # Dead code: warn if variables are assigned but never used (simple heuristic)
    for var in seen_vars:
        used = any(var in line for line in lines if not line.strip().startswith("#"))
        if not used:
            errors.append(f"{path.name}: variable '{var}' assigned but never used")

    return errors


def warn_file(path: Path) -> list[str]:
    warnings: list[str] = []
    lines = path.read_text(encoding="utf-8").splitlines()
    code_lines = [(idx, l.strip()) for idx, l in enumerate(lines, start=1) if l.strip() and not l.strip().startswith("#")]

    # Heuristic: always-on injected noise often sounds static-like.
    for pos, (line_no, line) in enumerate(code_lines):
        neighborhood = "\n".join(text for _, text in code_lines[max(0, pos - 2):pos + 1])
        if "noise(" in line and "if (" not in neighborhood and "select(" not in neighborhood:
            warnings.append(
                f"{path.name}:{line_no}: noise appears always-on; consider transient gating + slew/lpf1 smoothing for cleaner texture"
            )

    # Heuristic: very hard gate edges with pulse at tiny duty cycles can click.
    for pos, (line_no, line) in enumerate(code_lines):
        neighborhood = "\n".join(text for _, text in code_lines[max(0, pos - 2):pos + 3])
        if "pulse(" in line and ("0.00" in line or "0.01" in line) and "slew(" not in neighborhood:
            warnings.append(
                f"{path.name}:{line_no}: narrow pulse without nearby smoothing may click; consider slew()"
            )

    return warnings


def _collect_assignments(lines: Iterable[str]) -> dict[str, str]:
    assignments: dict[str, str] = {}
    for raw in lines:
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        if "=" not in line or not line.endswith(";"):
            continue
        lhs, rhs = line[:-1].split("=", 1)
        name = lhs.strip()
        if not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", name):
            continue
        assignments[name] = rhs.strip()
    return assignments


def audit_signal_safety(path: Path) -> list[str]:
    """Conservative static checks for likely broken/echo-prone behavior."""
    warnings: list[str] = []
    lines = path.read_text(encoding="utf-8").splitlines()
    assignments = _collect_assignments(lines)

    has_out_l = "outL" in assignments
    has_out_r = "outR" in assignments
    if not (has_out_l and has_out_r):
        warnings.append(f"{path.name}: missing explicit outL/outR assignment; stereo result may be unintended")

    if "delay(" in "\n".join(lines):
        fb_expr = assignments.get("fb")
        if fb_expr is not None:
            if not re.search(r"[A-Za-z_]", fb_expr):
                literals = [float(v) for v in re.findall(r"[-+]?\d*\.?\d+", fb_expr)]
                if literals and max(abs(v) for v in literals) >= 1.0:
                    warnings.append(
                        f"{path.name}: feedback expression '{fb_expr}' may reach unstable gain >= 1.0"
                    )

    mix_wet_refs = [rhs for name, rhs in assignments.items() if name in {"outL", "outR"} and "mix(" in rhs]
    if mix_wet_refs:
        if not any(re.search(r"\bp[1-8]\b", rhs) for rhs in mix_wet_refs):
            warnings.append(f"{path.name}: output mix appears fixed; consider exposing wet/dry macro control")

    return warnings


def main() -> int:
    scripts = sorted(EXAMPLES.glob("*.ascr"))
    if not scripts:
        print("No scripts found in examples/")
        return 1

    all_errors: list[str] = []
    all_warnings: list[str] = []
    for script in scripts:
        all_errors.extend(lint_file(script))
        all_warnings.extend(warn_file(script))
        all_warnings.extend(audit_signal_safety(script))

    if all_errors:
        print("Script validation failed:")
        for e in all_errors:
            print(f" - {e}")
        return 1

    if all_warnings:
        print("Script validation warnings:")
        for w in all_warnings:
            print(f" - {w}")

    print(f"Validated {len(scripts)} example scripts successfully.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
