#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2026 NXP
"""TIER 6 — anchor the harness to REAL SILICON.

NXP's fr1_fr2_test_tool ships md5 checksums of dumps captured from an actual
LA9310 chip (`known_waveform_list` in dump_time_domain_tx.sh / _rx.sh).  This
script reproduces three of them, byte for byte, with nothing but the native
in-process VSPA ISS -- no Docker, no Wine, no hardware.

What is reproduced
------------------
Stimulus (host side, from the test tool):
    ./send_single_tone.sh 0 <f_Hz> 25          # 25% of DAC full scale
    ./send_single_tone.sh 0 10000000 25 add    # (two-tone case only)
    ./dump_time_domain_tx.sh                   # 20 ms @ 61.44 Msps -> .bin

Chain point:  the TX ANTENNA BUFFER, i.e. the samples handed to the AXIQ DMA
for the DAC.  In single-tone mode the firmware bypasses the iFFT/upsampler and
fills the antenna buffer straight from the NCO kernels
(main_pair_coreA.c:449-461), so the antenna buffer content is exactly

    tone[n] = single_tone_asm(freq, phase=n, amp)      n = 0 .. 1_228_799

with n = offset_in_sframe/4, anchored at the sysperiod wrap (TIME_LEN_SYSPERIOD
= 40 ms, dump = the first 20 ms of it, offset_size = 0), so phase starts at 0.

File format (every byte of it matters for an md5):
  * size  = txaxiq_ksps * 4 B * 20 ms = 61440 * 4 * 20 = 4,915,200 B, no header
  * layout = interleaved int16 I,Q little-endian  (utils/calc_pwr.py)
  * encoding: the antenna buffer holds VSPA SIGN-MAGNITUDE 16; the dump DMA is
    issued with DMAC_WRC, documented in vspa-sdk/inc/dmac.h:47 as
    "Write to AXI and convert to 2's complement" -- so the file is two's
    complement.  That conversion is applied here, and it is load-bearing.

Result: all three md5s match NXP's hardware captures exactly.

Usage:  python3 tools/dfe_md5_anchor/run_md5_anchor.py [--quick]
        --quick skips the two-tone case (600 ISS kernel calls, ~60 s)
"""
from __future__ import annotations

import os

import argparse
import hashlib
import subprocess
import sys
import time
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[2]
sys.path.insert(0, str(REPO / "cosim"))

from native.session import NativeSession  # noqa: E402
from elftools.elf.elffile import ELFFile  # noqa: E402

# The probe is linked against the firmware exactly as the firmware links itself:
# same lcf, same lib, same crt0 objects out of firmware/build/ (so `make firmware`
# must have run first).
APP = REPO / "firmware"
ENV = Path(os.environ.get(
    "VSPA_TOOLS", REPO / "environment/cwvspa.vbeta_14_00_781_vspa.linux/VSPA_Tools"))
BUILD = HERE / "build"
ELD = BUILD / "single_tone_probe.eld"

SPS_KSPS = 61440          # TX AXIQ rate of the 61/61 LA9310 builds
NS = 2048                 # samples per kernel call (fits VCPU DRAM)
TOTAL = 1_228_800         # 20 ms @ 61.44 Msps
DUMP_BYTES = TOTAL * 4

#: md5 -> description, verbatim from nxp/fr1_fr2_test_tool/dump_time_domain_tx.sh
#: (the first two are ALSO listed in dump_time_domain_rx.sh)
CASES = [
    ("15kHz single tone", [(15_000, 25)], 4096,
     "bccbdab7f62763014a6d2026ee99ef44",
     "FDD single tone 15Khz 25% scale at 61Msps time domain dump"),
    ("30kHz single tone", [(30_000, 25)], 2048,
     "3186a4b1fcc7cc4375f90a788fb71b48",
     "FDD single tone 30Khz 25% scale at 61Msps time domain dump"),
    ("30kHz + 10MHz two-tone", [(30_000, 25), (10_000_000, 25)], TOTAL,
     "3ab0d88793f8dec448f873138ac0b536",
     "FDD single tone 30Khz+10Mhz 25% scale at 61Msps time domain dump"),
]


# ── stimulus encoding, exactly as the shell + firmware compute it ────────────
def nco_word(f_hz: int) -> int:
    """send_single_tone.sh:  freq = 4294967296 * f / sps_ksps / 1000."""
    return int(4294967296 * f_hz / SPS_KSPS / 1000) & 0xFFFFFFFF


def amp_word(percent: int) -> int:
    """send_single_tone.sh:   amp = 0x7FF * pct / 100   (bash integer div)
       host_vspa_msg.c:       a = (amp<<4) | (amp<<20) | SINGLE_TONE_VALID_FLAG
       main_pair_coreA.c:     single_tone_asm(..., a & ~SINGLE_TONE_VALID_FLAG, ...)"""
    p = (0x7FF * percent) // 100                 # 25% -> 511
    return ((p << 4) | (p << 20) | 1) & ~1       # -> 0x1FF01FF0


def sm16_to_twos(raw: bytes) -> bytes:
    """DMAC_WRC: "Write to AXI and convert to 2's complement" (dmac.h:47)."""
    u = np.frombuffer(raw, dtype="<u2").astype(np.int32)
    mag = u & 0x7FFF
    return np.where(u & 0x8000, -mag, mag).astype("<i2").tobytes()


def build() -> None:
    cc = ENV / "bin/fsvspacc"
    if not cc.exists():
        sys.exit(f"toolchain not found: {cc}  (run `make setup`)")
    BUILD.mkdir(parents=True, exist_ok=True)
    common = [
        str(cc), "-arch", "vspa2", "-au_count", "16", "-core_type", "sp",
        "-D__LA9310__", "-DLA9310", "-DVSPA2", "-DSELF_CORE_ID=0",
        "-DPROJ_CONFIG_OPTION=PROJ_1T1R_1CORE_LS_9310",
        "-O3", "-Os", "-opt=noalias_by_type",
        "-ansi", "off", "--keep", "-mvcpu", "-msgstyle", "gcc", "-env", str(ENV),
    ]
    incs = []
    for i in ("include", "lib", "src", "src/kernel_FFT", "src/function_Ant_manage"):
        incs += ["-I", str(APP / i)]
    incs += ["-I", str(ENV / "include"),
             "-I", str(REPO / "submodules/la931x_vspa_common/vspa-sdk/inc")]

    subprocess.run(common + incs + ["-c", str(HERE / "single_tone_probe.c"),
                                    "-o", str(BUILD)],
                   check=True, cwd=APP, capture_output=True)
    subprocess.run(common + ["-mem", "lcf/LA9310_sp.lcf", "-no-startup-files",
                             "-L", "lib",
                             str(BUILD / "single_tone_probe.eln"),
                             "build/boot.eln", "build/startup_vspa2_modified.eln",
                             "-l", "lib_DFE_Ref_vspa2.elb", "-o", str(ELD)],
                   check=True, cwd=APP, capture_output=True)


class ToneProbe:
    """One booted probe; each call() runs the real NCO kernel(s) on the ISS."""

    def __init__(self) -> None:
        with open(ELD, "rb") as f:
            self.sy = {s.name: s["st_value"]
                       for s in ELFFile(f).get_section_by_name(".symtab").iter_symbols()}
        self.s = NativeSession()
        self.s.load_elf(str(ELD))
        self.s.boot()
        self.s.run_phases(5000)          # settle into the go-poll loop
        self.done = self._rd("_done")

    def _rd(self, n): return self.s.read_words(self.sy[n], 1)[0]
    def _wr(self, n, v): self.s.write_words(self.sy[n], [v])

    def call(self, tones, phase: int) -> bytes:
        """One antenna buffer (NS samples) at sample offset ``phase``."""
        self._wr("_cfg_freq", nco_word(tones[0][0]))
        self._wr("_cfg_amp", amp_word(tones[0][1]))
        if len(tones) > 1:
            self._wr("_cfg_freq2", nco_word(tones[1][0]))
            self._wr("_cfg_amp2", amp_word(tones[1][1]))
        else:
            self._wr("_cfg_amp2", 0)
        self._wr("_cfg_lines", NS * 4 // 128)
        self._wr("_cfg_phase", phase)
        self._wr("_go", 1)
        for _ in range(400):
            self.s.run_phases(2000)
            if self._rd("_done") != self.done:
                break
        else:
            raise RuntimeError(f"kernel did not complete at phase {phase}")
        self.done = self._rd("_done")
        hw = self.s.read_halfwords(self.sy["_tone_buf"], NS * 2)
        return np.array(hw, dtype="<u2").tobytes()


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--quick", action="store_true",
                    help="skip the two-tone case (600 kernel calls, ~60 s)")
    args = ap.parse_args()

    build()
    p = ToneProbe()

    fails = 0
    for name, tones, period, want, desc in CASES:
        if args.quick and len(tones) > 1:
            print(f"  SKIP  {name} (--quick)")
            continue
        t0 = time.time()
        sm = b"".join(p.call(tones, k * NS) for k in range(period // NS))
        td = sm16_to_twos(sm)
        full = td * (DUMP_BYTES // len(td))
        assert len(full) == DUMP_BYTES, len(full)
        got = hashlib.md5(full).hexdigest()

        ok = got == want
        fails += not ok
        print(f"  {'PASS' if ok else 'FAIL'}  {name:24s} md5={got} "
              f"({time.time()-t0:.0f}s)")
        print(f"        {'== ' + desc if ok else '!= expected ' + want}")

    print("\nMATCHES REAL LA9310 SILICON" if not fails else "\nMISMATCH")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
