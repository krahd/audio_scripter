import pathlib
import sys
import unittest

TOOLS = pathlib.Path(__file__).resolve().parents[1] / "tools"
sys.path.insert(0, str(TOOLS))

from effect_ir_v0 import (  # noqa: E402
    Chance,
    Effect,
    Memory,
    Predicate,
    Schedule,
    Segmentation,
    Transformation,
    audio_input,
    bars,
    beats,
    combine,
    observe,
    process,
    rotate_behaviors,
)


class TestEffectIRV0(unittest.TestCase):
    def test_memory_can_record_input_and_transformed_output(self):
        src = audio_input()
        memory = Memory("past", bars(16))
        cloud = Transformation("cloud", "feedback", (("decay", 0.9),))
        voice = cloud.instantiate("voice-a")
        changed = process(voice, memory.material(age=bars(4)))

        memory.record(src)
        memory.record(changed, when=Chance(0.2))

        self.assertEqual([write.source.op for write in memory.writes], ["input", "process"])

    def test_fragment_query_is_independent_of_processor(self):
        src = audio_input()
        memory = Memory("past", bars(16))
        onset = observe(src, "onset")
        brightness = observe(src, "brightness")

        query = (
            memory.fragments(Segmentation("onset", onset))
            .nearest("brightness", brightness)
            .take(5)
            .every(2, "reverse")
        )

        self.assertEqual(query.nearest_feature.name, "brightness")
        self.assertEqual(query.limit, 5)
        self.assertNotIn("processor", repr(query).lower())

    def test_observation_supports_temporal_conditioning(self):
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

    def test_behavior_rotation_can_preserve_instance_identity(self):
        a = Transformation("a", "feedback").instantiate("A")
        b = Transformation("b", "resonator").instantiate("B")
        identities = (a.identity, b.identity)
        generations = (a.generation, b.generation)
        old_a, old_b = a.behavior, b.behavior

        a.replace_behavior(old_b, keep_state=True)
        b.replace_behavior(old_a, keep_state=True)

        self.assertEqual(identities, (a.identity, b.identity))
        self.assertEqual(generations, (a.generation, b.generation))
        self.assertEqual((a.behavior.name, b.behavior.name), ("b", "a"))

    def test_effect_families_use_ordinary_collections(self):
        base = Transformation("cloud", "grain", (("density", 1.0),))
        family = [
            base.derive(name=f"cloud-{index}", density=index / 4)
            for index in range(1, 5)
        ]

        self.assertEqual(len(family), 4)
        self.assertEqual(dict(family[-1].params)["density"], 1.0)

    def test_all_round_one_bakeoffs_construct_without_task_specific_primitives(self):
        src = audio_input()
        side = audio_input("side")
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

        definitions = [
            Transformation(f"t{index}", "grain", (("time_scale", scale),))
            for index, scale in enumerate([1.0, 0.5, 2.0, 1.5])
        ]
        voices = [
            transformation.instantiate(f"v{index}")
            for index, transformation in enumerate(definitions)
        ]
        rotation = Schedule(rotate_behaviors(voices, keep_state=True), every=bars(8))

        # Bake-off B: process selected remembered material and sometimes recapture it.
        altered = process(voices[0], selected.as_material())
        past.record(altered, when=Chance(0.2))

        # Bake-off C: temporally conditioned cross-adaptive observation relation.
        noise_a = observe(src, "noisiness").smooth(beats(0.5))
        noise_b = observe(side, "noisiness").smooth(beats(0.5))
        more_noisy = Predicate(noise_a, "above", noise_b).for_duration(beats(0.5))

        # Bake-off D: derive an effect family with ordinary collection syntax.
        family = [
            definitions[0].derive(name=f"variant-{index}", spread=index * 0.1)
            for index in range(6)
        ]

        effect = Effect(
            "round1",
            inputs=[src, side],
            memories=[past],
            instances=voices,
            observations=[brightness, noise_a, noise_b],
            schedules=[rotation, Schedule("change-mapping", when=more_noisy)],
        )
        effect.add_output(
            combine(*(process(voice, selected.as_material()) for voice in voices))
        )

        self.assertEqual(len(effect.memories[0].writes), 2)
        self.assertEqual(len(effect.instances), 4)
        self.assertEqual(len(family), 6)
        self.assertTrue(effect.schedules[0].action.keep_state)


if __name__ == "__main__":
    unittest.main()
