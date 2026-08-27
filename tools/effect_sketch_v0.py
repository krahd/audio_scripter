"""Concrete-abstraction / effect-sketch research prototype.

This is an authoring experiment on top of ``effect_ir_v1``. It is NOT proposed
surface syntax and NOT a novelty claim. The design deliberately follows the Elody /
concrete-abstraction and typed-hole / exploratory-programming lineages.

The question is narrower: can a concrete working Transformation be turned into an
executable sketch, have selected structure or values abstracted after discovery, and
then generate explicit variants without losing the original audible construction?

An abstracted slot retains the selected concrete structure/value as its default, so
creating the abstraction is initially semantics-preserving at the IR level.
"""

from __future__ import annotations

from dataclasses import dataclass, replace
from typing import Any, Iterable, Mapping

from effect_ir_v1 import Shape, Transformation, compose, parallel, primitive


@dataclass(frozen=True)
class Slot:
    """A typed authoring slot with a concrete default discovered in-place."""

    name: str
    kind: str
    default: Any

    def __post_init__(self) -> None:
        if self.kind not in {"transformation", "value"}:
            raise ValueError(f"unsupported slot kind: {self.kind}")
        if not self.name:
            raise ValueError("slot name must not be empty")


@dataclass(frozen=True)
class PrimitiveSpec:
    name: str
    params: tuple[tuple[str, Any], ...]
    input_shape: Shape
    output_shape: Shape
    state_schema: tuple[tuple[str, str], ...]


@dataclass(frozen=True)
class CompositeSpec:
    op: str
    name: str
    children: tuple["SketchExpr", ...]

    def __post_init__(self) -> None:
        if self.op not in {"compose", "parallel"}:
            raise ValueError(f"unsupported composite op: {self.op}")
        if not self.children:
            raise ValueError("composite sketch requires children")


SketchExpr = PrimitiveSpec | CompositeSpec | Slot
Path = tuple[int, ...]


def _from_transformation(transformation: Transformation) -> SketchExpr:
    expr = transformation.expr
    if expr.op == "primitive":
        name, params = expr.args
        return PrimitiveSpec(
            name=name,
            params=tuple(params),
            input_shape=transformation.input_shape,
            output_shape=transformation.output_shape,
            state_schema=transformation.state_schema,
        )

    if expr.op in {"compose", "parallel"}:
        children = tuple(_from_transformation(child) for child in expr.args)
        return CompositeSpec(expr.op, transformation.name, children)

    raise ValueError(f"effect_sketch_v0 cannot yet represent TransformExpr op {expr.op!r}")


def _resolve_value(value: Any, bindings: Mapping[str, Any]) -> Any:
    if not isinstance(value, Slot):
        return value
    if value.kind != "value":
        raise TypeError(f"transformation slot {value.name!r} used as a parameter value")
    resolved = bindings.get(value.name, value.default)
    if isinstance(resolved, Transformation):
        raise TypeError(f"value slot {value.name!r} cannot be filled by a Transformation")
    return resolved


def _compile(expr: SketchExpr, bindings: Mapping[str, Any]) -> Transformation:
    if isinstance(expr, Slot):
        if expr.kind != "transformation":
            raise TypeError(f"value slot {expr.name!r} used where a Transformation is required")
        chosen = bindings.get(expr.name, expr.default)
        if isinstance(chosen, Transformation):
            return chosen
        if isinstance(chosen, (PrimitiveSpec, CompositeSpec, Slot)):
            return _compile(chosen, bindings)
        raise TypeError(f"transformation slot {expr.name!r} requires a Transformation/sketch value")

    if isinstance(expr, PrimitiveSpec):
        params = {name: _resolve_value(value, bindings) for name, value in expr.params}
        return primitive(
            expr.name,
            input_channels=expr.input_shape.channels,
            output_channels=expr.output_shape.channels,
            state_schema=expr.state_schema,
            **params,
        )

    children = tuple(_compile(child, bindings) for child in expr.children)
    if expr.op == "compose":
        return compose(*children, name=expr.name)
    return parallel(*children, name=expr.name)


def _node_at(expr: SketchExpr, path: Path) -> SketchExpr:
    node = expr
    for index in path:
        if not isinstance(node, CompositeSpec):
            raise IndexError("path descends through a non-composite sketch node")
        if index < 0 or index >= len(node.children):
            raise IndexError(f"sketch path index out of range: {index}")
        node = node.children[index]
    return node


def _replace_node(expr: SketchExpr, path: Path, replacement: SketchExpr) -> SketchExpr:
    if not path:
        return replacement
    if not isinstance(expr, CompositeSpec):
        raise IndexError("path descends through a non-composite sketch node")
    index = path[0]
    if index < 0 or index >= len(expr.children):
        raise IndexError(f"sketch path index out of range: {index}")
    children = list(expr.children)
    children[index] = _replace_node(children[index], path[1:], replacement)
    return replace(expr, children=tuple(children))


def _replace_param(expr: SketchExpr, path: Path, param_name: str, replacement: Any) -> SketchExpr:
    node = _node_at(expr, path)
    if not isinstance(node, PrimitiveSpec):
        raise TypeError("parameter abstraction requires a primitive node")

    params = dict(node.params)
    if param_name not in params:
        raise KeyError(f"primitive {node.name!r} has no parameter {param_name!r}")
    params[param_name] = replacement
    changed = replace(node, params=tuple(sorted(params.items())))
    return _replace_node(expr, path, changed)


def _collect_slots(expr: SketchExpr, found: dict[str, Slot]) -> None:
    if isinstance(expr, Slot):
        existing = found.get(expr.name)
        if existing is not None and existing != expr:
            raise ValueError(f"slot name {expr.name!r} is reused with incompatible definitions")
        found[expr.name] = expr
        if isinstance(expr.default, (PrimitiveSpec, CompositeSpec, Slot)):
            _collect_slots(expr.default, found)
        return

    if isinstance(expr, PrimitiveSpec):
        for _, value in expr.params:
            if isinstance(value, Slot):
                existing = found.get(value.name)
                if existing is not None and existing != value:
                    raise ValueError(
                        f"slot name {value.name!r} is reused with incompatible definitions"
                    )
                found[value.name] = value
        return

    for child in expr.children:
        _collect_slots(child, found)


@dataclass(frozen=True)
class EffectSketch:
    """Structurally explicit, always-defaultable sketch of a Transformation."""

    root: SketchExpr

    @classmethod
    def from_transformation(cls, transformation: Transformation) -> "EffectSketch":
        return cls(_from_transformation(transformation))

    def slots(self) -> tuple[Slot, ...]:
        found: dict[str, Slot] = {}
        _collect_slots(self.root, found)
        return tuple(found.values())

    def abstract_transformation(self, path: Path, name: str) -> "EffectSketch":
        selected = _node_at(self.root, path)
        if isinstance(selected, Slot):
            raise TypeError("selected structure is already a slot")
        return EffectSketch(
            _replace_node(
                self.root,
                path,
                Slot(name=name, kind="transformation", default=selected),
            )
        )

    def abstract_value(self, path: Path, param_name: str, name: str) -> "EffectSketch":
        selected = _node_at(self.root, path)
        if not isinstance(selected, PrimitiveSpec):
            raise TypeError("value abstraction requires selecting a primitive")
        params = dict(selected.params)
        if param_name not in params:
            raise KeyError(f"primitive {selected.name!r} has no parameter {param_name!r}")
        slot = Slot(name=name, kind="value", default=params[param_name])
        return EffectSketch(_replace_param(self.root, path, param_name, slot))

    def instantiate(self, bindings: Mapping[str, Any] | None = None, /, **named: Any) -> Transformation:
        merged: dict[str, Any] = {}
        if bindings is not None:
            merged.update(bindings)
        merged.update(named)

        known = {slot.name for slot in self.slots()}
        unknown = set(merged) - known
        if unknown:
            raise KeyError(f"unknown sketch slot(s): {', '.join(sorted(unknown))}")
        return _compile(self.root, merged)

    def variants(self, slot_name: str, alternatives: Iterable[Any]) -> tuple[Transformation, ...]:
        slots = {slot.name: slot for slot in self.slots()}
        if slot_name not in slots:
            raise KeyError(f"unknown sketch slot: {slot_name}")
        return tuple(self.instantiate({slot_name: alternative}) for alternative in alternatives)
