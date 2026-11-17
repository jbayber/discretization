"""Python bindings for the discretization C++ library.

All functions accept any 1-D array-like of numbers: NumPy arrays, pandas
Series, or plain lists.

:func:`calculate_discretization_factor` and :func:`discretize` work in
``float32``: a C-contiguous ``float32`` NumPy array is passed to C++ without
copying; anything else is converted to one first.

:func:`best_discretization_factor` dispatches on the input's dtype:
``float32`` input runs in float32; anything else (``float64``, lists,
``float16``, integers) runs in float64, never narrowed to float32. Contiguous
``float32`` and ``float64`` arrays are passed without copying. For example
``best_discretization_factor([0.3]).factor`` is ``10``, while a ``float32``
``[0.3]`` gives ``16777216`` (the exact float32 value, with the unlimited
default budget).

``unit_system`` selects the candidate factors: ``"metric"`` (default) or
``"imperial"`` (case-insensitive), or a :class:`UnitSystem` member.
:func:`best_discretization_factor` also accepts ``None`` (the default: search
both lists, recommended when the unit system is unknown) and returns a
read-only :class:`FactorResult`.
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
