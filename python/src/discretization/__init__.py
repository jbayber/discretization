"""Python bindings for the discretization C++ library.

Both functions accept any 1-D array-like of numbers: NumPy arrays, pandas
Series, or plain lists. A C-contiguous ``float32`` NumPy array is passed to
C++ without copying; anything else is converted to one first.
"""

from ._core import calculate_discretization_factor, discretize

__all__ = ["calculate_discretization_factor", "discretize"]
