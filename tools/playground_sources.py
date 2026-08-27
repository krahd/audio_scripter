"""Source helpers for the offline playground and authoring-audition tools.

Supports ordinary RIFF/WAVE integer PCM and IEEE floating-point WAV files without external
dependencies. WAVE_FORMAT_EXTENSIBLE is accepted when its subformat is PCM or IEEE float.
Linear resampling is used when a source rate differs from a study rate.
"""

from __future__ import annotations

from pathlib import Path
import struct

from playground_v0 import RenderContext


WAVE_FORMAT_PCM = 0x0001
WAVE_FORMAT_IEEE_FLOAT = 0x0003
WAVE_FORMAT_EXTENSIBLE = 0xFFFE


def _riff_chunks(raw: bytes):
    if len(raw) < 12 or raw[:4] != b"RIFF" or raw[8:12] != b"WAVE":
        raise ValueError("not a RIFF/WAVE file")

    offset = 12
    while offset + 8 <= len(raw):
        chunk_id = raw[offset : offset + 4]
        size = struct.unpack_from("<I", raw, offset + 4)[0]
        start = offset + 8
        end = start + size
        if end > len(raw):
            raise ValueError(f"truncated WAV chunk {chunk_id!r}")
        yield chunk_id, raw[start:end]
        offset = end + (size & 1)


def _normalise_format_tag(fmt: bytes, format_tag: int) -> int:
    if format_tag != WAVE_FORMAT_EXTENSIBLE:
        return format_tag

    # WAVEFORMATEXTENSIBLE = WAVEFORMATEX (18 bytes) + valid bits (2) +
    # channel mask (4) + 16-byte subtype GUID. The first 32-bit GUID field is
    # WAVE_FORMAT_PCM (1) or WAVE_FORMAT_IEEE_FLOAT (3) for the formats we accept.
    if len(fmt) < 40:
        raise ValueError("truncated WAVE_FORMAT_EXTENSIBLE fmt chunk")
    return struct.unpack_from("<I", fmt, 24)[0]


def _decode_integer_sample(chunk: bytes, bits: int) -> float:
    if bits == 8:
        return (chunk[0] - 128) / 128.0
    if bits == 16:
        return struct.unpack("<h", chunk)[0] / 32768.0
    if bits == 24:
        value = int.from_bytes(chunk, byteorder="little", signed=False)
        if value & 0x800000:
            value -= 1 << 24
        return value / float(1 << 23)
    if bits == 32:
        return struct.unpack("<i", chunk)[0] / float(1 << 31)
    raise ValueError(f"unsupported integer PCM bit depth: {bits}")


def _decode_float_sample(chunk: bytes, bits: int) -> float:
    if bits == 32:
        return float(struct.unpack("<f", chunk)[0])
    if bits == 64:
        return float(struct.unpack("<d", chunk)[0])
    raise ValueError(f"unsupported IEEE-float bit depth: {bits}")


def read_wav_mono(path: str | Path) -> tuple[list[float], int]:
    """Read common uncompressed WAV formats and average channels to mono.

    Accepted sample formats:
    - PCM unsigned 8-bit;
    - PCM signed 16/24/32-bit;
    - IEEE float 32/64-bit;
    - WAVE_FORMAT_EXTENSIBLE wrapping PCM/IEEE float.

    The helper is intentionally small and rejects compressed WAV codecs explicitly.
    """

    path = Path(path)
    raw = path.read_bytes()

    fmt: bytes | None = None
    data: bytes | None = None
    for chunk_id, payload in _riff_chunks(raw):
        if chunk_id == b"fmt " and fmt is None:
            fmt = payload
        elif chunk_id == b"data" and data is None:
            data = payload

    if fmt is None:
        raise ValueError("WAV has no fmt chunk")
    if data is None:
        raise ValueError("WAV has no data chunk")
    if len(fmt) < 16:
        raise ValueError("truncated WAV fmt chunk")

    format_tag, channels, rate, _byte_rate, block_align, bits = struct.unpack_from(
        "<HHIIHH", fmt, 0
    )
    format_tag = _normalise_format_tag(fmt, format_tag)

    if channels < 1:
        raise ValueError("WAV must have at least one channel")
    if rate <= 0:
        raise ValueError("WAV sample rate must be positive")
    if bits % 8 != 0:
        raise ValueError(f"unsupported non-byte-aligned WAV bit depth: {bits}")

    width = bits // 8
    expected_align = channels * width
    if block_align < expected_align:
        raise ValueError("WAV block alignment is smaller than one sample frame")
    if block_align == 0:
        raise ValueError("WAV block alignment must be positive")

    if format_tag == WAVE_FORMAT_PCM:
        decode = lambda chunk: _decode_integer_sample(chunk, bits)
    elif format_tag == WAVE_FORMAT_IEEE_FLOAT:
        decode = lambda chunk: _decode_float_sample(chunk, bits)
    else:
        raise ValueError(f"unsupported WAV format tag: {format_tag}")

    samples: list[float] = []
    for frame_start in range(0, len(data) - block_align + 1, block_align):
        total = 0.0
        for channel in range(channels):
            start = frame_start + channel * width
            total += decode(data[start : start + width])
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
