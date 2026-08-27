import pathlib
import sys
import unittest

TOOLS = pathlib.Path(__file__).resolve().parents[1] / "tools"
sys.path.insert(0, str(TOOLS))

from effect_ir_v1 import (  # noqa: E402
    Chance,
    Effect,
    Memory,
    Policy,
    Predicate,
    Schedule,
    Segmentation,
    StateMigration,
    ago,
    apply,
    audio_input,
    bars,
    beats,
    combine,
    compose,
    observe,
    primitive,
    process,
    rate,
    reverse,
    rotate_policies,
)


class TestEffectIRV1(unittest.TestCase):
    def test_memory_reads_use_temporal_addresses(self):
        src = audio_input(channels=2)
        memory = Memory("past", bars(16))
        memory.record(src)

        fixed = memory.read(ago(bars(4)))
        backwards = memory.read(reverse(window=beats(2)))
        half_speed = memory.read(rate(0.5, region="older"))

        self.assertEqual(fixed.args[1].op, "ago")
        self.assertEqual(backwards.args[1].op, "reverse")
        self.assertEqual(half_speed.args[1].op, "rate")
        self.assertEqual(fixed.shape.channels, 2)

    def test_simple_dsp_composition_needs_no_effect_or_memory(self):
        src = audio_input(channels=2)
        gain = primitive("gain", amount=2.0)
        saturate = primitive("saturate", drive=0.7)
        lowpass = primitive("lowpass", cutoff=1200)
        chain = compose(gain, saturate, lowpass, name="simple")

        result = apply(chain, src)

        self.assertEqual(result.shape.channels, 2)
        self.assertEqual(chain.expr.op, "compose")
        self.assertEqual(len(chain.expr.args), 3)

    def test_policy_rotation_preserves_heterogeneous_processor_state_identity(self):
        src = audio_input()
        old = Memory("past", bars(16))
        old.record(src)

        feedback = primitive("feedback", state_schema=(("z", "float"),))
        resonator = primitive("resonator", state_schema=(("y1", "float"), ("y2", "float")))
        a = feedback.instantiate("A", policy=Policy(material=src, controls=(("scale", 1.0),)))
        b = resonator.instantiate("B", policy=Policy(material=old.read(ago(bars(4))), controls=(("scale", 2.0),)))

        transforms_before = (a.transformation, b.transformation)
        generations_before = (a.generation, b.generation)
        policies_before = (a.policy, b.policy)

        rotate_policies([a, b])

        self.assertEqual((a.transformation, b.transformation), transforms_before)
        self.assertEqual((a.generation, b.generation), generations_before)
        self.assertEqual((a.policy, b.policy), (policies_before[1], policies_before[0]))

    def test_algorithm_replacement_resets_state_by_default(self):
        old = primitive("feedback", state_schema=(("z", "float"),))
        new = primitive("resonator", state_schema=(("y1", "float"), ("y2", "float")))
        instance = old.instantiate("A")

        instance.replace_transformation(new)

        self.assertEqual(instance.transformation, new)
        self.assertEqual(instance.generation, 1)
        self.assertIsNone(instance.last_migration)

    def test_explicit_state_migration_is_representable_and_checked(self):
        old = primitive("old", state_schema=(("x", "float"),))
        new = primitive("new", state_schema=(("y", "float"),))
        instance = old.instantiate("A")
        migration = StateMigration("x-to-y", old.state_schema, new.state_schema)

        instance.replace_transformation(new, migration=migration)

        self.assertEqual(instance.generation, 0)
        self.assertEqual(instance.last_migration, migration)

    def test_observation_temporal_conditioning_still_works(self):
        src = audio_input()
        sparse = (
            observe(src, "activity")
            .smooth(beats(0.25))
            .below(0.2)
            .for_duration(beats(1))
            .hysteresis(0.03)
        )

        self.assertIn(("dwell", beats(1)), sparse.temporal_ops)
        self.assertIn(("hysteresis", 0.03), sparse.temporal_ops)

    def test_round_one_bakeoffs_still_construct(self):
        src = audio_input("in", channels=2)
        side = audio_input("side", channels=2)
        past = Memory("past", bars(16))
        past.record(src)

        onset = observe(src, "onset")
        brightness = observe(src, "brightness")
        sparse = observe(src, "activity").below(0.2).for_duration(beats(1))
        selected = (
            past.fragments(Segmentation("onset", onset))
            .nearest("brightness", brightness)
            .take(5)
            .every(2, "reverse")
            .prefer("older", when=sparse)
        )
        selected_material = selected.as_material(shape=src.shape)

        definitions = [
            primitive("grain", time_scale=scale)
            for scale in [1.0, 0.5, 2.0, 1.5]
        ]
        policies = [Policy(material=selected_material, controls=(("time_scale", scale),)) for scale in [1.0, 0.5, 2.0, 1.5]]
        voices = [
            definition.instantiate(f"v{index}", policy=policies[index])
            for index, definition in enumerate(definitions)
        ]

        # A: collection-heavy remembered material + persistent transformations.
        rotate_schedule = Schedule("rotate-policies", every=bars(8))

        # B: transform then recapture.
        altered = process(voices[0])
        past.record(altered, when=Chance(0.2))

        # C: cross-adaptive temporally conditioned observation.
        noise_a = observe(src, "noisiness").smooth(beats(0.5))
        noise_b = observe(side, "noisiness").smooth(beats(0.5))
        relation = Predicate(noise_a, "above", noise_b).for_duration(beats(0.5))

        # D: families remain ordinary programming over Transformation values.
        family = [primitive("grain", spread=index * 0.1) for index in range(6)]

        effect = Effect(
            "round1",
            inputs=[src, side],
            memories=[past],
            instances=voices,
            observations=[brightness, noise_a, noise_b],
            schedules=[rotate_schedule, Schedule("change-mapping", when=relation)],
        )
        effect.add_output(combine(*(process(voice) for voice in voices)))

        self.assertEqual(len(effect.memories[0].writes), 2)
        self.assertEqual(len(effect.instances), 4)
        self.assertEqual(len(family), 6)

    def test_stress_suite_representative_cases_need_no_new_root_types(self):
        src = audio_input(channels=4)
        past = Memory("past", bars(32))
        past.record(src)

        # S1/S3: normal DSP + iterated family.
        delays = [primitive("delay", distance=beats(index + 1), feedback=0.8 - index * 0.05) for index in range(6)]
        iterated = compose(*delays, name="iterated")

        # S4/S6: long memory and alternate temporal views.
        old = past.read(ago(bars(8)))
        backwards = past.read(reverse(window=beats(2)))
        slowed = past.read(rate(0.5, region="oldest-4-bars"))

        # S7/S8: segmentation/observation.
        onset = observe(src, "onset")
        fragments = past.fragments(Segmentation("onset", onset)).take(8)

        # S11: cross-adaptive relation can be a Predicate.
        relation = observe(src, "brightness").above(observe(old, "brightness"))

        # S12/S15: shared memory and non-stereo material shapes.
        a = primitive("cloud").instantiate("A", policy=Policy(material=fragments.as_material(shape=src.shape)))
        b = primitive("lossy").instantiate("B", policy=Policy(material=backwards))

        outputs = combine(process(a), process(b), apply(iterated, slowed))

        self.assertEqual(src.shape.channels, 4)
        self.assertEqual(outputs.shape.channels, 4)
        self.assertEqual(fragments.limit, 8)
        self.assertIsInstance(relation, Predicate)


if __name__ == "__main__":
    unittest.main()
