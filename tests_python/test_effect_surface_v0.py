import pathlib
import sys
import unittest

TOOLS = pathlib.Path(__file__).resolve().parents[1] / "tools"
sys.path.insert(0, str(TOOLS))

from effect_ir_v1 import compose, parallel, primitive  # noqa: E402
from effect_sketch_v0 import EffectSketch  # noqa: E402
from effect_surface_v0 import (  # noqa: E402
    render_sketch,
    render_structural_diff,
    render_transformation,
    render_variants,
    structural_diff,
)


class TestEffectSurfaceV0(unittest.TestCase):
    def setUp(self):
        self.delay = primitive("delay", distance=0.25, feedback=0.6)
        self.reverb = primitive("reverb", size=0.7)
        self.saturate = primitive("saturate", drive=0.3)
        self.effect = compose(self.delay, self.reverb, self.saturate, name="found")

    def test_concrete_transformation_uses_readable_function_and_arrow_projection(self):
        rendered = render_transformation(self.effect)

        self.assertEqual(
            rendered,
            "delay(distance=0.25, feedback=0.6) -> reverb(size=0.7) -> saturate(drive=0.3)",
        )
        self.assertNotIn("|>", rendered)

    def test_concrete_abstraction_projects_defaults_inside_explicit_audio_effect_frame(self):
        sketch = (
            EffectSketch.from_transformation(self.effect)
            .abstract_transformation((1,), "space")
            .abstract_value((0,), "feedback", "recurrence")
        )

        rendered = render_sketch(sketch, name="found")

        self.assertIn("slot space = reverb(size=0.7)", rendered)
        self.assertIn("slot recurrence = 0.6", rendered)
        self.assertIn("effect found(in):", rendered)
        self.assertIn(
            "in -> delay(distance=0.25, feedback=recurrence) -> space -> saturate(drive=0.3) -> out",
            rendered,
        )

    def test_structural_diff_localises_replaced_subtransformation(self):
        sketch = EffectSketch.from_transformation(self.effect).abstract_transformation((1,), "space")
        cloud = parallel(
            primitive("reverb", size=0.9),
            primitive("grain", density=4),
            name="cloud",
        )
        variant = sketch.instantiate(space=cloud)

        changes = structural_diff(self.effect, variant)

        self.assertEqual(len(changes), 1)
        self.assertEqual(changes[0].path, (1,))
        self.assertEqual(changes[0].before, "reverb(size=0.7)")
        self.assertIn("parallel(", changes[0].after)

    def test_parameter_change_is_reported_at_readable_stage_location(self):
        sketch = EffectSketch.from_transformation(self.effect).abstract_value((0,), "feedback", "recurrence")
        variant = sketch.instantiate(recurrence=0.9)

        rendered = render_structural_diff(self.effect, variant)

        self.assertIn("stage[0]:", rendered)
        self.assertIn("feedback=0.6", rendered)
        self.assertIn("feedback=0.9", rendered)

    def test_parallel_variants_are_visible_and_compared_against_discovered_default(self):
        sketch = EffectSketch.from_transformation(self.effect).abstract_transformation((1,), "space")
        rendered = render_variants(
            sketch,
            "space",
            [
                ("dryer", primitive("reverb", size=0.2)),
                (
                    "cloud",
                    parallel(
                        primitive("reverb", size=0.8),
                        primitive("pitch_shift", semitones=7),
                    ),
                ),
            ],
        )

        self.assertIn("variant dryer:", rendered)
        self.assertIn("variant cloud:", rendered)
        self.assertIn("changes:", rendered)
        self.assertIn("stage[1]: reverb(size=0.7)", rendered)


if __name__ == "__main__":
    unittest.main()
