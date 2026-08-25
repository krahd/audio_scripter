import importlib.util
from pathlib import Path
import sys


MODULE_PATH = Path(__file__).resolve().parents[1] / "tools" / "lifecycle_spike.py"
SPEC = importlib.util.spec_from_file_location("lifecycle_spike", MODULE_PATH)
assert SPEC is not None and SPEC.loader is not None
lifecycle_spike = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = lifecycle_spike
SPEC.loader.exec_module(lifecycle_spike)


ReferenceTransformation = lifecycle_spike.ReferenceTransformation
MemoryTransformation = lifecycle_spike.MemoryTransformation
BeatPolicy = lifecycle_spike.BeatPolicy
Lifecycle = lifecycle_spike.Lifecycle
InputPolicy = lifecycle_spike.InputPolicy
StatePolicy = lifecycle_spike.StatePolicy
OutputPolicy = lifecycle_spike.OutputPolicy
HistoryPolicy = lifecycle_spike.HistoryPolicy
ObservationPolicy = lifecycle_spike.ObservationPolicy
NORMAL = lifecycle_spike.NORMAL
TAIL = lifecycle_spike.TAIL
FREEZE = lifecycle_spike.FREEZE
SILENT_EVOLUTION = lifecycle_spike.SILENT_EVOLUTION
HISTORY_ONLY = lifecycle_spike.HISTORY_ONLY
HOLD_ALL = lifecycle_spike.HOLD_ALL
OBSERVE_ONLY = lifecycle_spike.OBSERVE_ONLY


def test_tail_blocks_new_input_but_state_decays_and_remains_audible():
    t = ReferenceTransformation(decay=0.5)
    assert t.process_sample(1.0, NORMAL) == 1.0
    outputs = [t.process_sample(1.0, TAIL) for _ in range(3)]
    assert outputs == [0.5, 0.25, 0.125]
    assert t.state == 0.125


def test_freeze_preserves_state_exactly():
    t = ReferenceTransformation(decay=0.5)
    assert t.process_sample(1.0, NORMAL) == 1.0
    outputs = [t.process_sample(1.0, FREEZE) for _ in range(3)]
    assert outputs == [1.0, 1.0, 1.0]
    assert t.state == 1.0


def test_silent_evolution_advances_state_while_suppressing_output():
    t = ReferenceTransformation(decay=0.5)
    assert t.process_sample(1.0, NORMAL) == 1.0
    assert t.process_sample(1.0, SILENT_EVOLUTION) == 0.0
    assert t.state == 1.5
    assert t.process_sample(0.0, NORMAL) == 0.75


def test_history_only_accumulates_source_without_advancing_processing_state():
    t = ReferenceTransformation(decay=0.5, history_size=4)
    assert t.process_sample(1.0, NORMAL) == 1.0
    frozen_state = t.state
    assert t.process_sample(2.0, HISTORY_ONLY) == 0.0
    assert t.process_sample(3.0, HISTORY_ONLY) == 0.0
    assert t.state == frozen_state
    assert t.recent_history()[-2:] == (2.0, 3.0)
    assert t.observation > 0.0


def test_resume_preserves_identity_and_distinguishes_tail_from_freeze():
    tail = ReferenceTransformation(decay=0.5)
    frozen = ReferenceTransformation(decay=0.5)
    tail.process_sample(1.0, NORMAL)
    frozen.process_sample(1.0, NORMAL)
    tail.process_sample(0.0, TAIL)
    frozen.process_sample(0.0, FREEZE)
    assert tail.state == 0.5
    assert frozen.state == 1.0
    assert tail.process_sample(0.0, NORMAL) == 0.25
    assert frozen.process_sample(0.0, NORMAL) == 0.5


def test_transition_invariants_preserve_identity_and_owned_state_contract():
    t = ReferenceTransformation(decay=0.5)
    identity = id(t)
    assert t.process_sample(1.0, NORMAL) == 1.0
    assert t.process_sample(9.0, TAIL) == 0.5
    frozen = t.state
    assert t.process_sample(9.0, FREEZE) == frozen
    assert id(t) == identity
    assert t.state == frozen
    assert t.process_sample(0.0, NORMAL) == 0.25


def test_pathological_policy_combinations_remain_semantically_distinct():
    silent = ReferenceTransformation(decay=0.5)
    observe = ReferenceTransformation(decay=0.5)
    held = ReferenceTransformation(decay=0.5)
    for t in (silent, observe, held):
        t.process_sample(1.0, NORMAL)

    assert silent.process_sample(1.0, SILENT_EVOLUTION) == 0.0
    assert silent.state == 1.5

    history_before = observe.recent_history()
    state_before = observe.state
    observation_before = observe.observation
    assert observe.process_sample(4.0, OBSERVE_ONLY) == 0.0
    assert observe.state == state_before
    assert observe.recent_history() == history_before
    assert observe.observation > observation_before

    history_before = held.recent_history()
    state_before = held.state
    observation_before = held.observation
    assert held.process_sample(9.0, HOLD_ALL) == 0.0
    assert held.state == state_before
    assert held.recent_history() == history_before
    assert held.observation == observation_before


def test_musical_time_policy_changes_do_not_recreate_processor():
    policy = BeatPolicy()
    t = ReferenceTransformation(decay=0.5)
    identity = id(t)
    states = []
    for beat in range(20):
        t.process_sample(1.0, policy.at(beat))
        states.append(t.state)
        assert id(t) == identity

    assert states[12] == states[11]
    assert states[13] == states[12]
    assert states[14] == states[13]
    assert states[15] == states[14]
    assert states[16] != states[15]


def test_second_stateful_process_obeys_same_lifecycle_contract():
    t = MemoryTransformation(history_size=4)
    identity = id(t)
    t.process_sample(1.0, NORMAL)
    t.process_sample(2.0, NORMAL)
    phase = t.read_phase
    t.process_sample(3.0, FREEZE)
    assert id(t) == identity
    assert t.read_phase == phase
    t.process_sample(4.0, SILENT_EVOLUTION)
    assert t.read_phase != phase


def test_history_accumulated_while_process_frozen_later_becomes_audible():
    t = MemoryTransformation(history_size=4)
    for value in (10.0, 20.0, 30.0, 40.0):
        assert t.process_sample(value, HISTORY_ONLY) == 0.0
    assert t.read_phase == 0

    read_only = Lifecycle(
        input=InputPolicy.BLOCK,
        state=StatePolicy.ADVANCE,
        output=OutputPolicy.AUDIBLE,
        history=HistoryPolicy.HOLD,
        observation=ObservationPolicy.HOLD,
    )
    outputs = [t.process_sample(999.0, read_only) for _ in range(4)]
    assert outputs == [20.0, 30.0, 40.0, 10.0]
