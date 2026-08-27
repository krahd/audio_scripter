import pathlib
import sys
import unittest

TOOLS = pathlib.Path(__file__).resolve().parents[1] / "tools"
sys.path.insert(0, str(TOOLS))

from effect_ir_v1 import compose, parallel, primitive  # noqa: E402
from effect_sketch_v0 import EffectSketch  # noqa: E402


class TestEffectSketchV0(unittest.TestCase):
    def setUp(self):
        self.delay = primitive(
            "delay",
            distance=0.25,
            feedback=0.6,
            state_schema=(("buffer", "delay"),),
        )
        self.reverb = primitive(
            "reverb",
            size=0.7,
            decay=0.8,
            state_schema=(("tank", "reverb"),),
        )
        self.saturate = primitive("saturate", drive=0.3)
        self.effect = compose(self.delay, self.reverb, self.saturate, name="found-effect")

    def test_round_trip_preserves_concrete_transformation(self):
        sketch = EffectSketch.from_transformation(self.effect)
        rebuilt = sketch.instantiate()

        self.assertEqual(rebuilt.expr, self.effect.expr)
        self.assertEqual(rebuilt.state_schema, self.effect.state_schema)
        self.assertEqual(rebuilt.name, self.effect.name)

    def test_abstracting_concrete_substructure_is_initially_semantics_preserving(self):
        sketch = EffectSketch.from_transformation(self.effect)
        sketch = sketch.abstract_transformation((1,), "space")

        rebuilt = sketch.instantiate()

        self.assertEqual(rebuilt.expr, self.effect.expr)
        self.assertEqual([slot.name for slot in sketch.slots()], ["space"])

    def test_transformation_slot_can_be_filled_with_discovered_alternative(self):
        sketch = (
            EffectSketch.from_transformation(self.effect)
            .abstract_transformation((1,), "space")
        )
        cloud = parallel(
            primitive("reverb", size=0.9),
            primitive("pitch_shift", semitones=7),
            name="cloud",
        )

        variant = sketch.instantiate(space=cloud)

        self.assertEqual(variant.expr.args[1], cloud)
        self.assertEqual(variant.expr.args[0].name, "delay")
        self.assertEqual(variant.expr.args[2].name, "saturate")

    def test_value_abstraction_keeps_discovered_value_as_default(self):
        sketch = EffectSketch.from_transformation(self.effect)
        sketch = sketch.abstract_value((0,), "feedback", "recurrence")

        unchanged = sketch.instantiate()
        changed = sketch.instantiate(recurrence=0.91)

        self.assertEqual(unchanged.expr, self.effect.expr)
        delay_params = dict(changed.expr.args[0].expr.args[1])
        self.assertEqual(delay_params["feedback"], 0.91)

    def test_nested_structure_can_be_abstracted_after_discovery(self):
        inner = compose(self.reverb, self.saturate, name="colour")
        nested = compose(self.delay, inner, name="nested")
        sketch = EffectSketch.from_transformation(nested)
        sketch = sketch.abstract_transformation((1, 0), "inside-colour")

        replacement = primitive("granulator", size=0.08)
        variant = sketch.instantiate({"inside-colour": replacement})

        colour = variant.expr.args[1]
        self.assertEqual(colour.expr.args[0].name, "granulator")
        self.assertEqual(colour.expr.args[1].name, "saturate")

    def test_explicit_variant_family_is_structural_not_parameter_snapshot_magic(self):
        sketch = (
            EffectSketch.from_transformation(self.effect)
            .abstract_transformation((1,), "space")
        )
        alternatives = [
            primitive("reverb", size=0.2),
            primitive("reverse_reverb", size=0.7),
            parallel(primitive("reverb", size=0.8), primitive("grain", density=4)),
        ]

        variants = sketch.variants("space", alternatives)

        self.assertEqual(len(variants), 3)
        self.assertEqual(variants[0].expr.args[1].name, "reverb")
        self.assertEqual(variants[1].expr.args[1].name, "reverse_reverb")
        self.assertEqual(variants[2].expr.args[1].expr.op, "parallel")

    def test_wrong_slot_kind_is_rejected(self):
        sketch = (
            EffectSketch.from_transformation(self.effect)
            .abstract_value((0,), "feedback", "recurrence")
        )

        with self.assertRaises(TypeError):
            sketch.instantiate(recurrence=primitive("gain", amount=2))

    def test_unknown_binding_is_rejected(self):
        sketch = EffectSketch.from_transformation(self.effect)

        with self.assertRaises(KeyError):
            sketch.instantiate(does_not_exist=1)


if __name__ == "__main__":
    unittest.main()
