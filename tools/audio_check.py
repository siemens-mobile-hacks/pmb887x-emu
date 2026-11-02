#!/usr/bin/env python3

import argparse
import math
from dataclasses import dataclass
from pathlib import Path
import struct
import sys

import numpy as np


EXPECT_PREFIX = "# AUDIO-EXPECT: "
WINDOW_MS = 20
SILENCE_DB = -40
EDGE_TRIM_MS = 100
FREQUENCY_SEARCH = 0.1
FREQUENCY_TOLERANCE = 0.005
MIN_FUNDAMENTAL_DB = -30
PRESENT_FUNDAMENTAL_DB = -20
DURATION_TOLERANCE_MS = 100
PERIODICITY_WINDOW_MS = 100
MAX_PERIOD_DIFFERENCE = 0.01


@dataclass
class Recording:
    rate_hz: int
    samples: np.ndarray


@dataclass
class Tone:
    frequency_hz: int
    source_rate_hz: int
    duration_ms: int


@dataclass
class Segment:
    start: int
    end: int


def read_wav(path):
    data = Path(path).read_bytes()
    if data[0:4] != b"RIFF" or data[8:12] != b"WAVE":
        raise ValueError(f"{path}: not a WAV file")

    offset = 12
    channels = rate_hz = bits = None
    while offset + 8 <= len(data):
        chunk_id = data[offset:offset + 4]
        chunk_size = struct.unpack_from("<I", data, offset + 4)[0]
        body = offset + 8
        if chunk_id == b"fmt ":
            _, channels, rate_hz, _, _, bits = struct.unpack_from("<HHIIHH", data, body)
        elif chunk_id == b"data":
            # QEMU leaves the data size at zero when it is stopped by a signal.
            end = body + chunk_size if chunk_size != 0 else len(data)
            if channels is None or bits != 16:
                raise ValueError(f"{path}: expected 16-bit PCM")
            frames = np.frombuffer(data[body:end], dtype="<i2")
            frames = frames[:len(frames) // channels * channels].reshape(-1, channels)
            return Recording(rate_hz, frames.astype(np.float64).mean(axis=1))
        offset = body + chunk_size + (chunk_size & 1)
    raise ValueError(f"{path}: no data chunk")


def parse_expectations(lines):
    tones = []
    for line in lines:
        words = line.split()
        if not words:
            continue
        fields = dict(word.split("=", 1) for word in words[1:])
        if words[0] != "TONE":
            raise ValueError(f"unknown audio expectation: {line}")
        tones.append(Tone(int(fields["hz"]), int(fields["rate"]), int(fields["ms"])))
    return tones


def spectrum_peak(samples, rate_hz, low_hz, high_hz):
    padded = len(samples) * 4
    spectrum = np.abs(np.fft.rfft(samples * np.hanning(len(samples)), n=padded))
    bin_hz = rate_hz / padded
    low = max(int(low_hz / bin_hz), 1)
    high = min(int(high_hz / bin_hz) + 1, len(spectrum) - 1)
    peak = low + int(np.argmax(spectrum[low:high]))
    left, center, right = np.log(spectrum[peak - 1:peak + 2] + 1e-12)
    offset = 0.5 * (left - right) / (left - 2 * center + right)
    level_db = 20 * np.log10(spectrum[peak] / spectrum[1:].max())
    return (peak + offset) * bin_hz, level_db


def find_tone_segments(recording, min_tone_ms):
    window = recording.rate_hz * WINDOW_MS // 1000
    count = len(recording.samples) // window
    windows = recording.samples[:count * window].reshape(count, window)
    rms = np.sqrt(((windows - windows.mean(axis=1, keepdims=True)) ** 2).mean(axis=1))
    active = rms > rms.max() * 10 ** (SILENCE_DB / 20)

    segments = []
    for index in np.nonzero(active)[0]:
        if segments and segments[-1].end == index * window:
            segments[-1].end += window
        else:
            segments.append(Segment(index * window, (index + 1) * window))

    min_length = recording.rate_hz * min_tone_ms // 1000
    return [segment for segment in segments if segment.end - segment.start >= min_length]


def repetition_frames(rate_hz, tone):
    # A periodic source repeats exactly every 1 / gcd(tone, source rate) seconds, whatever the chain did to its shape.
    repetition_hz = math.gcd(tone.frequency_hz, tone.source_rate_hz)
    if rate_hz % repetition_hz != 0:
        raise ValueError(f"{tone.frequency_hz} Hz at {tone.source_rate_hz} Hz does not repeat on {rate_hz} Hz samples")
    return rate_hz // repetition_hz


def period_difference(samples, period):
    current = samples[period:]
    previous = samples[:-period]
    return np.sum((current - previous) ** 2) / np.sum(current ** 2)


def fundamental_duration_ms(recording, segment, frequency_hz):
    window = recording.rate_hz * WINDOW_MS // 1000
    count = (segment.end - segment.start) // window
    windows = recording.samples[segment.start:segment.start + count * window].reshape(count, window)
    carrier = np.exp(-2j * np.pi * frequency_hz * np.arange(window) / recording.rate_hz)
    amplitude = np.abs(windows @ carrier)
    present = np.nonzero(amplitude >= np.median(amplitude) * 10 ** (PRESENT_FUNDAMENTAL_DB / 20))[0]
    return (present[-1] - present[0] + 1) * WINDOW_MS


def check_tone(recording, segment, expected):
    rate_hz = recording.rate_hz
    trim = rate_hz * EDGE_TRIM_MS // 1000
    body = recording.samples[segment.start + trim:segment.end - trim]
    frequency_hz, level_db = spectrum_peak(body, rate_hz,
        expected.frequency_hz * (1 - FREQUENCY_SEARCH), expected.frequency_hz * (1 + FREQUENCY_SEARCH))
    duration_ms = fundamental_duration_ms(recording, segment, expected.frequency_hz)

    period = repetition_frames(rate_hz, expected)
    window = rate_hz * PERIODICITY_WINDOW_MS // 1000 + period
    worst_difference = max(
        period_difference(body[start:start + window], period)
        for start in range(0, len(body) - window + 1, window - period)
    )

    failures = []
    if abs(frequency_hz - expected.frequency_hz) > expected.frequency_hz * FREQUENCY_TOLERANCE:
        failures.append(f"frequency {frequency_hz:.1f} Hz")
    if level_db < MIN_FUNDAMENTAL_DB:
        failures.append(f"fundamental {level_db:.1f} dB")
    if abs(duration_ms - expected.duration_ms) > DURATION_TOLERANCE_MS:
        failures.append(f"duration {duration_ms} ms")
    if worst_difference > MAX_PERIOD_DIFFERENCE:
        failures.append(f"period difference {worst_difference * 100:.2f}%")

    start_ms = segment.start * 1000 // rate_hz
    summary = (f"tone {expected.frequency_hz} Hz for {expected.duration_ms} ms at {start_ms} ms: "
        f"{frequency_hz:.1f} Hz at {level_db:.1f} dB, {duration_ms} ms, period difference {worst_difference * 100:.2f}%")
    return failures, summary


def check_recording(path, expectation_lines):
    recording = read_wav(path)
    expected = parse_expectations(expectation_lines)
    # Firmware output stages settle between tones; anything much shorter than a tone is not one of them.
    segments = find_tone_segments(recording, min(tone.duration_ms for tone in expected) // 2)
    messages = []
    passed = True

    if len(segments) != len(expected):
        found = ", ".join(f"{(s.end - s.start) * 1000 // recording.rate_hz} ms" for s in segments)
        return False, [f"expected {len(expected)} tones, recorded {len(segments)} sounds: {found or 'silence'}"]

    for segment, tone in zip(segments, expected):
        failures, summary = check_tone(recording, segment, tone)
        messages.append(summary if not failures else f"{summary} (bad {', '.join(failures)})")
        passed = passed and not failures
    return passed, messages


def main():
    parser = argparse.ArgumentParser(description="Check a WAV recording against AUDIO-EXPECT lines.")
    parser.add_argument("wav", type=Path)
    parser.add_argument("log", type=Path, help="test output containing AUDIO-EXPECT lines")
    args = parser.parse_args()

    lines = [
        line[len(EXPECT_PREFIX):]
        for line in args.log.read_text(errors="replace").splitlines()
        if line.startswith(EXPECT_PREFIX)
    ]
    passed, messages = check_recording(args.wav, lines)
    for message in messages:
        print(f"# audio: {message}")
    return 0 if passed else 1


if __name__ == "__main__":
    sys.exit(main())
