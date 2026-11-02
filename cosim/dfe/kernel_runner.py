# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2026 NXP
"""NativeKernelRunner — run a single VSPA kernel on the in-process ISS.

This is the kernel-level counterpart of ``DfeSession`` (which drives the whole DFE
firmware image). It exists so a host can push arrays through one kernel at a time
— which is what a link-level co-simulation needs: TX → channel → RX, with the
channel modelled in Python between two real VSPA runs.

WHY THERE ARE NO FILES HERE
---------------------------
The old runner talked to a separate ``runsim`` process, so the only way to hand a
kernel an array was through the filesystem: Python wrote ``input.bin``, the
kernel's C code ``fopen``/``fread``-ed it, computed, ``fwrite``-ed ``output.bin``,
and printed a marker line that Python scraped out of stdout.

That cannot work here, and the reason is worth stating plainly: **the native ISS
services only three syscalls — write, clock and time.** ``open`` and ``read``
return -1 (see ``NativeModel.service_syscall``). A file-backed harness gets a NULL
``FILE *``.

It also isn't needed. The simulator runs *inside this process*, so its memory is
simply there to be written: resolve the kernel's buffers from the ELF symbol table
and poke them. No files, no recompile per call, no stdout scraping.

THE ONE SUBTLETY: crt0 ZEROES BSS
---------------------------------
A kernel's buffers are ordinary C globals, so they live in ``.bss`` — and the
startup code (``_init_bss``) zeroes ``.bss`` *after* the host has loaded the image
but *before* ``main`` runs. Anything written between ``load_elf`` and ``boot`` is
therefore wiped, silently: the kernel computes on zeros and returns a clean,
plausible, wrong answer.

So inputs are written at a breakpoint on ``main`` — past crt0, before the kernel
has looked at them. That keeps the C sources free of placement tricks (no forced
initialisers to push a buffer out of ``.bss``) and works whatever the linker does.
"""
from __future__ import annotations

import sys
from dataclasses import dataclass
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from native.model import NativeModel, SPACE_DRAM  # noqa: E402
from dfe.session import to_sm16, from_sm16        # noqa: E402


@dataclass(frozen=True)
class Port:
    """One kernel I/O buffer.

    ``symbol`` is the C name of the buffer (the compiler's leading underscore is
    added automatically). ``n`` is its element count; ``enc`` how an element is
    packed into the 32-bit DMEM word:

      sm16_complex — cfixed16_t.  Im_sm16 << 16 | Re_sm16.  What every VSPA
                     *assembly* kernel reads and writes: sign in bit 15,
                     magnitude in bits 14:0 — NOT two's complement.
      chf_complex  — cfloat16_t.  Im_fp16 << 16 | Re_fp16: a pair of IEEE
                     half-floats.  Do NOT confuse with sm16_complex — the bit
                     patterns are both 16+16 but the numbers are unrelated, and
                     mixing them up is silent, not an error.  chEst's pilot and
                     the equaliser's output are this type.
      csp_complex  — cfloat32_t.  TWO words per element: Re_fp32 then Im_fp32.
                     rcpv's equaliser weights are this type, which is why a
                     256-element buffer of them is 2048 B, not 1024.
      u32          — raw, no interpretation.

    ``n`` is the ELEMENT count.  For csp_complex that is half the word count —
    see ``nwords``.
    """
    name: str
    symbol: str
    n: int
    enc: str = "sm16_complex"

    @property
    def nwords(self) -> int:
        """DMEM words the buffer occupies. One per element, except cfloat32_t,
        which is 8 bytes = two words."""
        return 2 * self.n if self.enc == "csp_complex" else self.n


def _word_reverse(data: bytes) -> bytes:
    out = bytearray(len(data))
    for i in range(0, len(data), 4):
        out[i:i + 4] = data[i:i + 4][::-1]
    return bytes(out)


def encode(values: np.ndarray, enc: str) -> list[int]:
    if enc == "u32":
        return [int(v) & 0xFFFFFFFF for v in np.asarray(values).ravel()]
    if enc == "sm16_complex":
        v = np.asarray(values).ravel()
        re = to_sm16(np.round(np.real(v)).astype(np.int32))
        im = to_sm16(np.round(np.imag(v)).astype(np.int32))
        return [int(i) << 16 | int(r) for r, i in zip(re, im)]
    if enc == "chf_complex":
        v = np.asarray(values, dtype=np.complex128).ravel()
        re = np.real(v).astype(np.float16).view(np.uint16)
        im = np.imag(v).astype(np.float16).view(np.uint16)
        return [int(i) << 16 | int(r) for r, i in zip(re, im)]
    if enc == "csp_complex":
        v = np.asarray(values, dtype=np.complex128).ravel()
        re = np.real(v).astype(np.float32).view(np.uint32)
        im = np.imag(v).astype(np.float32).view(np.uint32)
        out: list[int] = []
        for r, i in zip(re, im):          # real word first, then imag
            out.append(int(r))
            out.append(int(i))
        return out
    raise ValueError(f"unknown encoding {enc!r}")


def decode(words: list[int], enc: str) -> np.ndarray:
    if enc == "u32":
        return np.array(words, dtype=np.uint32)
    if enc == "sm16_complex":
        w = np.array(words, dtype=np.uint32)
        return from_sm16(w & 0xFFFF) + 1j * from_sm16((w >> 16) & 0xFFFF)
    if enc == "chf_complex":
        w = np.array(words, dtype=np.uint32)
        re = (w & 0xFFFF).astype(np.uint16).view(np.float16).astype(np.float64)
        im = ((w >> 16) & 0xFFFF).astype(np.uint16).view(np.float16).astype(np.float64)
        return re + 1j * im
    if enc == "csp_complex":
        w = np.array(words, dtype=np.uint32)
        re = w[0::2].copy().view(np.float32).astype(np.float64)
        im = w[1::2].copy().view(np.float32).astype(np.float64)
        return re + 1j * im
    raise ValueError(f"unknown encoding {enc!r}")


class NativeKernelRunner:
    """Run one kernel ELF, repeatedly, in this process.

    The ELF is loaded fresh per call — that is a memcpy into the simulator's DRAM,
    not a process spawn, and it re-initialises ``.data`` and re-zeroes ``.bss`` so
    every call starts from the same state.

        runner = NativeKernelRunner(
            "cosim/kernels/build/compute_dfe_tx_vspa2_16au.eld",
            inputs=[Port("input_sc", "input_sc_packed", 61)],
            outputs=[Port("output_td", "work_buf_out", 1024)])
        td = runner.compute({"input_sc": sc})["output_td"]
    """

    def __init__(self, elf: str | Path, inputs: list[Port], outputs: list[Port],
                 model: NativeModel | None = None) -> None:
        self.elf = Path(elf)
        if not self.elf.exists():
            raise FileNotFoundError(f"kernel ELF not built: {self.elf}")
        self.inputs = list(inputs)
        self.outputs = list(outputs)
        # One device serves every runner in the process — the model DLL keeps
        # per-device state and a second device would clobber the first.
        self.m = model or _shared_model()
        self.stdout = ""
        self.phases = 0

    def compute(self, values: dict[str, np.ndarray]) -> dict[str, np.ndarray]:
        m = self.m
        missing = {p.name for p in self.inputs} - set(values)
        if missing:
            raise ValueError(f"missing input port(s): {sorted(missing)}")

        m.reset_core()
        entry = m.load_elf(self.elf)
        m.stdout.clear()

        # Halt just inside main: crt0 has zeroed .bss by now, and the kernel has
        # not read its inputs yet. (See the module docstring — writing before
        # boot would be silently zeroed back out.)
        bp = m.set_breakpoint(m.symbol("main") // 4)
        m.boot(entry)
        reason, p1 = m.run_to_exit()
        if reason != "trap":
            raise RuntimeError(
                f"{self.elf.name}: expected to halt at main, got {reason!r} "
                f"after {p1} phases. stdout={''.join(m.stdout)!r}")
        m.clear_breakpoint(bp)

        for port in self.inputs:
            v = np.asarray(values[port.name]).ravel()
            if v.size != port.n:
                raise ValueError(f"port {port.name!r} expects {port.n} elements, "
                                 f"got {v.size}")
            words = encode(v, port.enc)
            raw = b"".join(w.to_bytes(4, "little") for w in words)
            m.set_space(SPACE_DRAM)
            m._write_span(m.symbol(port.symbol), _word_reverse(raw), 0, unit=4)

        reason, p2 = m.run_to_exit()
        self.stdout = "".join(m.stdout)
        self.phases = p1 + p2
        if reason != "done":
            raise RuntimeError(
                f"{self.elf.name}: kernel did not reach a clean exit "
                f"({reason!r} after {self.phases} phases). "
                f"stdout={self.stdout!r}")

        return {p.name: decode(m.read_dram_words(m.symbol(p.symbol), p.nwords),
                               p.enc)
                for p in self.outputs}


_MODEL: NativeModel | None = None


def _shared_model() -> NativeModel:
    global _MODEL
    if _MODEL is None:
        _MODEL = NativeModel()
    return _MODEL
