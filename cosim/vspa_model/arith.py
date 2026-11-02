# SPDX-License-Identifier: BSD-3-Clause
"""Compatibility shim for the submodule-local shared VSPA arithmetic package."""

from pathlib import Path
import sys


_COMMON_PYTHON = Path(__file__).resolve().parents[2] / 'submodules' / 'la931x_vspa_common' / 'vspa-lib' / 'common' / 'python'
if str(_COMMON_PYTHON) not in sys.path:
    sys.path.insert(0, str(_COMMON_PYTHON))

from vspa.arith import *  # noqa: F401,F403
from vspa.arith import _f32_add_trunc, _f32_to_f16_trunc, _f64_to_f32_trunc  # noqa: F401

__all__ = [name for name in globals() if not name.startswith('_COMMON_PYTHON')]
