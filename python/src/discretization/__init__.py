"""Python bindings for the discretization C++ library.

All functions accept any 1-D array-like of numbers: NumPy arrays, pandas
Series, or plain lists. A C-contiguous ``float32`` NumPy array is passed to
C++ without copying; anything else is converted to one first.

``unit_system`` selects the candidate factors: ``"metric"`` (default) or
``"imperial"`` (case-insensitive), or a :class:`UnitSystem` member.
:func:`best_discretization_factor` also accepts ``None`` (search both lists)
and returns a read-only :class:`FactorResult`.
"""

from ._core import (
    FactorResult,
    UnitSystem,
    best_discretization_factor,
    calculate_discretization_factor,
    discretize,
)

__all__ = [
    "FactorResult",
    "UnitSystem",
    "best_discretization_factor",
    "calculate_discretization_factor",
    "discretize",
]
