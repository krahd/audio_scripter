import pathlib
import sys
import unittest

TOOLS = pathlib.Path(__file__).resolve().parents[1] / "tools"
sys.path.insert(0, str(TOOLS))

from effect_audition_v0 import (  # noqa: E402
    compile_processor,
    render_samples,
    render_sketch,
)
from effect_ir_v1 import compose, parallel, primitive  # noqa: E402
from effect_sketch_v0 import EffectSketch  # noqa: E402


class TestEffectAuditionV0(unittest.TestCase):
    def test_gain_is_exact(self):
        effect = primitive("gain", amount=2.0)
        result = render_samples(effect, [0.1, -0.2, 0.0], sample_rate=1000)
        self.assertEqual(result, [0.2, -0.4, 0.0])

    def test_delay_keeps_dry_signal_and_produces_echo(self):
        effect = primitive("delay", distance=0.01, feedback=0.5, mix=0.5)
        source = [1.0] + [0.0] * 30
        result = render_samples(effect, source, sample_rate=1000)

        self.assertAlmostEqual(result[0], 0.5)
        self.assertAlmostEqual(result[10], 0.5)
        self.assertAlmostEqual(result[20], 0.25)

    def test_reverb_produces_nonzero_tail(self):
        effect = primitive("reverb", size=0.7, decay=0.8)
        source = [1.0] + [0.0] * 99
        result = render_samples(effect, source, sample_rate=1000, tail_seconds=0.2)

        self.assertEqual(len(result), 300)
        self.assertTrue(any(abs(sample) > 1e-6 for sample in result[100:]))

    def test_serial_and_parallel_structures_are_auditionable(self):
        serial = compose(
            primitive("gain", amount=0.5),
            primitive("saturate", drive=0.1),
        )
        parallel_effect = parallel(
            primitive("gain", amount=1.0),
            primitive("gain", amount=0.5),
        )

        serial_result = render_samples(serial, [0.25], sample_rate=1000)
        parallel_result = render_samples(parallel_effect, [0.4], sample_rate=1000)

        self.assertEqual(len(serial_result), 1)
        self.assertAlmostEqual(parallel_result[0], 0.3)

    def test_concrete_abstraction_default_auditions_identically(self):
        concrete = compose(
            primitive("delay", distance=0.01, feedback=0.4, mix=0.4),
            primitive("reverb", size=0.5, decay=0.7),
            primitive("saturate", drive=0.05),
            name="concrete",
        )
        sketch = EffectSketch.from_transformation(concrete).abstract_transformation((1,), "space")
        source = [0.4, -0.2, 0.1] + [0.0] * 100

        direct = render_samples(concrete, source, sample_rate=1000, tail_seconds=0.1)
        through_sketch = render_sketch(
            sketch,
            source,
            sample_rate=1000,
            tail_seconds=0.1,
        )

        self.assertEqual(direct, through_sketch)

    def test_structural_variant_changes_audio_without_changing_surrounding_structure(self):
        concrete = compose(
            primitive("delay", distance=0.01, feedback=0.4, mix=0.4),
            primitive("reverb", size=0.5, decay=0.7),
            primitive("saturate", drive=0.05),
            name="concrete",
        )
        sketch = EffectSketch.from_transformation(concrete).abstract_transformation((1,), "space")
        alternative = parallel(
            primitive("reverb", size=1.0, decay=0.9),
            primitive("lowpass", cutoff=120.0),
            name="dark-space",
        )
        source = [0.4, -0.2, 0.1] + [0.0] * 100

        baseline = render_sketch(sketch, source, sample_rate=1000, tail_seconds=0.1)
        variant = render_sketch(
            sketch,
            source,
            sample_rate=1000,
            bindings={"space": alternative},
            tail_seconds=0.1,
        )

        self.assertNotEqual(baseline, variant)

    def test_unsupported_primitive_fails_explicitly(self):
        with self.assertRaises(NotImplementedError):
            compile_processor(primitive("granulator"), sample_rate=1000)


if __name__ == "__main__":
    unittest.main()
