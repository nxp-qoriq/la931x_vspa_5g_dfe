# The VSPA firmware

NXP's DFE reference application, stripped to the one configuration this repository
targets: **LA9310, single VSPA2 core, 16 AU, 1T2R, 25 MHz bandwidth, 30 kHz SCS**
(61 subcarriers TX / 61 RX).

```bash
make -C firmware      # -> firmware/eld/dfe-la9310-1t2r-25m30k.eld
```

Builds natively with `fsvspacc`. No Docker, no Wine, no CodeWarrior.

> **This half is NXP's.** Edit as little as possible; the simulation half is where our work belongs.

## What is in here

| | |
|---|---|
| `src/` | the application — TX and RX chains, antenna buffer management, host messaging |
| `include/` | its headers |
| `lib/` | `lib_DFE_Ref_vspa2.elb`, NXP's DSP library (the real iFFT, FFT, filters, NCO, QEC), plus its headers |
| `lcf/` | `LA9310_sp.lcf` — the memory map |
| `prebuilt/` | two object files that cannot be rebuilt on Linux (below) |

The build is configured entirely by preprocessor defines:

```
-D__LA9310__ -DLA9310 -DVSPA2 -DSELF_CORE_ID=0
-DPROJ_CONFIG_OPTION=PROJ_1T1R_1CORE_LS_9310
```

The original DFE reference is a multi-core VSPA3/LA1224 project; the LA9310 build
config is the single-core one, and it is the only one wired up here.

## The two prebuilt objects

`prebuilt/` exists because two translation units cannot be compiled by the Linux
toolchain. They are checked in as `.eln` objects, built by CodeWarrior on Windows:

| object | why |
|---|---|
| `inter_core_msg.eln` | internal compiler error in backend codegen (`fscgg_vspa.cpp:282`) |
| `kernel_FFT/ifft_cp.eln` | `fsvspacc` cannot resolve IPPU function addresses at compile time |

Their sources are still here (`src/inter_core_msg.c`, `src/kernel_FFT/ifft_cp.c`) —
they just aren't in the compile list. If a later toolchain fixes either bug, drop the
object from `PREBUILT_OBJS` and add the source to `LOCAL_C_SRCS`.

`src/startup_vspa2_modified.s` is a locally modified crt0; the stock one is in the
SDK.

## Linker warnings are expected

The link emits `W2003: found undefined input symbol` for a handful of symbols
(`_gen_decx2_NTaps_align_asm`, `_bitRev_fft_4096_b0_b1_wrap_cp_insert`, …). These are
entry points for configurations this build does not use — 4096-point FFT, other
antenna counts. They are never called on the 1T2R 25M/30K path, and the image is
correct: it is **bit-exact against NXP's own reference build** across 18
vector/config cases, and matches MD5s captured from real LA9310 silicon. See
[`../docs/TESTING.md`](../docs/TESTING.md).

## Is it right?

Don't take the build's word for it — the whole point of [`../cosim/`](../cosim/) is
to answer that question. `make verify-full` from the repo root runs the tiers.
