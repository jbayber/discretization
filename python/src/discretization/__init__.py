"""Python bindings for the discretization C++ library.

Both functions accept any 1-D array-like of numbers: NumPy arrays, pandas
Series, or plain lists. A C-contiguous ``float32`` NumPy array is passed to
C++ without copying; anything else is converted to one first.

``unit_system`` selects the candidate factors: ``"metric"`` (default) or
``"imperial"`` (case-insensitive), or a :class:`UnitSystem` member.
"""

from ._core import UnitSystem, calculate_discretization_factor, discretize

__all__ = ["UnitSystem", "calculate_discretization_factor", "discretize"]
