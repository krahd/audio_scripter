"""Throwaway textual projection for effect-authoring trajectory experiments.

NOT proposed .ascr syntax. This module exists only to make the structured effect/sketch
prototype readable enough to evaluate viscosity, concrete abstraction, variants, and
structural provenance. It intentionally uses familiar function-call notation and ``->``
for serial transformation composition; no syntax decision is implied.
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import Any, Iterable

from effect_ir_v1 import TransformExpr, Transformation
from effect_sketch_v0 import CompositeSpec, EffectSketch, PrimitiveSpec, SketchExpr, Slot


def _fmt_number(value: float) -> str:
    return f"{value:g}"


def format_value(value: Any) -> str:
    if isinstance(value, float):
        return _fmt_number(value)
    if isinstance(value, str):
        return repr(value)
    if isinstance(value, Slot):
        return value.name
    if isinstance(value, Transformation):
        return render_transformation(value)
    if isinstance(value, (list, tuple)):
        left, right = ("[", "]") if isinstance(value, list) else ("(", ")")
        return left + ", ".join(format_value(item) for item in value) + right
    return str(value)


def _render_primitive_spec(spec: PrimitiveSpec) -> str:
    params = ", ".join(f"{name}={format_value(value)}" for name, value in spec.params)
    return f"{spec.name}({params})" if params else f"{spec.name}()"


def _render_sketch_expr(expr: SketchExpr, parent_op: str | None = None) -> str:
    if isinstance(expr, Slot):
        return expr.name
    if isinstance(expr, PrimitiveSpec):
        return _render_primitive_spec(expr)

    rendered = [_render_sketch_expr(child, expr.op) for child in expr.children]
    if expr.op == "compose":
        text = " -> ".join(rendered)
    else:
        text = "parallel(" + ", ".join(rendered) + ")"

    if parent_op is not None and parent_op != expr.op:
        return f"({text})"
    return text


def render_sketch(sketch: EffectSketch, *, name: str = "effect") -> str:
    """Render a readable textual projection of a sketch and its concrete defaults."""

    slots = sketch.slots()
    lines: list[str] = []

    for slot in slots:
        if slot.kind == "transformation":
            default = _render_sketch_expr(slot.default)
        else:
            default = format_value(slot.default)
        lines.append(f"slot {slot.name} = {default}")

    if lines:
        lines.append("")

    lines.append(f"effect {name}:")
    lines.append(f"    {_render_sketch_expr(sketch.root)}")
    return "\n".join(lines)


def _render_transform_expr(expr: TransformExpr, parent_op: str | None = None) -> str:
    if expr.op == "primitive":
        name, params = expr.args
        rendered = ", ".join(f"{key}={format_value(value)}" for key, value in params)
        return f"{name}({rendered})" if rendered else f"{name}()"

    if expr.op in {"compose", "parallel"}:
        children = [render_transformation(child, parent_op=expr.op) for child in expr.args]
        if expr.op == "compose":
            text = " -> ".join(children)
        else:
            text = "parallel(" + ", ".join(children) + ")"
        if parent_op is not None and parent_op != expr.op:
            return f"({text})"
        return text

    return f"{expr.op}(...)"


def render_transformation(
    transformation: Transformation,
    *,
    parent_op: str | None = None,
) -> str:
    return _render_transform_expr(transformation.expr, parent_op)


@dataclass(frozen=True)
class StructuralChange:
    path: tuple[int, ...]
    before: str
    after: str

    def render(self) -> str:
        location = "root" if not self.path else ".".join(str(index) for index in self.path)
        return f"{location}: {self.before} -> {self.after}"


def _transformation_children(transformation: Transformation) -> tuple[Transformation, ...]:
    if transformation.expr.op in {"compose", "parallel"}:
        return tuple(transformation.expr.args)
    return ()


def structural_diff(
    before: Transformation,
    after: Transformation,
    *,
    path: tuple[int, ...] = (),
) -> tuple[StructuralChange, ...]:
    """Return a compact structural diff between two concrete transformations.

    This is intentionally structural rather than a text-line diff. It is research tooling for
    comparing explicit variants and does not attempt semantic equivalence.
    """

    if before == after:
        return ()

    b_children = _transformation_children(before)
    a_children = _transformation_children(after)

    same_container = (
        before.expr.op == after.expr.op
        and before.expr.op in {"compose", "parallel"}
        and len(b_children) == len(a_children)
    )

    if same_container:
        changes: list[StructuralChange] = []
        for index, (b_child, a_child) in enumerate(zip(b_children, a_children)):
            changes.extend(structural_diff(b_child, a_child, path=path + (index,)))
        if changes:
            return tuple(changes)

    return (
        StructuralChange(
            path=path,
            before=render_transformation(before),
            after=render_transformation(after),
        ),
    )


def render_structural_diff(before: Transformation, after: Transformation) -> str:
    changes = structural_diff(before, after)
    if not changes:
        return "no structural change"
    return "\n".join(change.render() for change in changes)


def render_variants(
    sketch: EffectSketch,
    slot_name: str,
    alternatives: Iterable[tuple[str, Any]],
) -> str:
    """Project explicit slot alternatives plus their structural difference from default."""

    baseline = sketch.instantiate()
    lines: list[str] = []
    for label, alternative in alternatives:
        variant = sketch.instantiate({slot_name: alternative})
        lines.append(f"variant {label}:")
        lines.append(f"    {render_transformation(variant)}")
        lines.append("    changes:")
        for diff_line in render_structural_diff(baseline, variant).splitlines():
            lines.append(f"        {diff_line}")
    return "\n".join(lines)
