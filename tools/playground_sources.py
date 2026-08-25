"""Source helpers for the offline playground.

The first implementation deliberately supports only ordinary PCM WAV files and uses
linear resampling when the source sample rate differs from the study rate. No external
dependencies are required.
"""

from __future__ import annotations

from pathlib import Path
import struct
import wave

from playground_v0 import RenderContext


def read_wav_mono(path: str | Path) -> tuple[list[float], int]:
    path = Path(path)
    with wave.open(str(path), "rb") as wf:
        channels = wf.getnchannels()
        width = wf.getsampwidth()
        rate = wf.getframerate()
        frames = wf.getnframes()
        raw = wf.readframes(frames)

    if channels < 1:
        raise ValueError("WAV must have at least one channel")
    if width not in (1, 2, 3, 4):
        raise ValueError(f"unsupported PCM sample width: {width}")

    frame_width = channels * width
    samples: list[float] = []

    def decode(chunk: bytes) -> float:
        if width == 1:
            return (chunk[0] - 128) / 128.0
        if width == 2:
            return struct.unpack("<h", chunk)[0] / 32768.0
        if width == 3:
            value = int.from_bytes(chunk, byteorder="little", signed=False)
            if value & 0x800000:
                value -= 1 << 24
            return value / float(1 << 23)
        return struct.unpack("<i", chunk)[0] / float(1 << 31)

    for frame_start in range(0, len(raw), frame_width):
        total = 0.0
        for channel in range(channels):
            start = frame_start + channel * width
            total += decode(raw[start : start + width])
        samples.append(total / channels)

    return samples, rate


def wav_source(
    path: str | Path,
    *,
    output_sample_rate: int,
    loop: bool = True,
):
    samples, input_rate = read_wav_mono(path)
    if not samples:
        raise ValueError("WAV contains no samples")

    ratio = input_rate / output_sample_rate

    def source(ctx: RenderContext) -> float:
        pos = ctx.sample_index * ratio
        if loop:
            pos %= len(samples)
        elif pos >= len(samples) - 1:
            return 0.0

        i0 = int(pos)
        frac = pos - i0
        i1 = (i0 + 1) % len(samples) if loop else min(i0 + 1, len(samples) - 1)
        return samples[i0] * (1.0 - frac) + samples[i1] * frac

    return source
