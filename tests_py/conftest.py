from typing import Callable

import numpy as np
import pandas as pd
import pytest

from tests_py.constants import Constants


@pytest.fixture
def values_float32() -> np.ndarray:
    return np.array(Constants.VALUES, dtype=np.float32)


@pytest.fixture
def series() -> pd.Series:
    return pd.Series(Constants.VALUES)


@pytest.fixture
def dataframe() -> pd.DataFrame:
    return pd.DataFrame({"x": Constants.VALUES})


@pytest.fixture(scope="session")
def large_values() -> np.ndarray:
    rng = np.random.default_rng(seed=0)
    return rng.uniform(-1e6, 1e6, Constants.BENCHMARK_SIZE).astype(np.float32)


def round_half_away_from_zero(x: np.ndarray) -> np.ndarray:
    """Reference rounding with std::round semantics (np.round rounds half to even).

    ``x - trunc(x)`` is exact in floating point, so the halfway test is exact too.
    """
    truncated = np.trunc(x)
    fraction = x - truncated
    return truncated + np.where(np.abs(fraction) >= 0.5, np.sign(x), 0.0)


@pytest.fixture(scope="session")
def property_values() -> np.ndarray:
    rng = np.random.default_rng(seed=Constants.PROPERTY_SEED)
    return rng.uniform(
        Constants.PROPERTY_LOW, Constants.PROPERTY_HIGH, Constants.PROPERTY_SIZE
    ).astype(np.float32)


@pytest.fixture(scope="session")
def fits() -> Callable[[np.ndarray, int, float], bool]:
    """Reference check of whether `factor` fits `values` within `epsilon`.

    Mirrors the spec: float32 inputs (and epsilon, which C++ takes as float)
    converted to double, q = round(v * f) halfway away from zero, and
    |q / f - v| <= epsilon for every value.
    """

    def _fits(values, factor: int, epsilon: float) -> bool:
        v = np.asarray(values, dtype=np.float32).astype(np.float64)
        eps = float(np.float32(epsilon))
        q = round_half_away_from_zero(v * factor)
        return bool(np.all(np.abs(q / factor - v) <= eps))

    return _fits


@pytest.fixture(scope="session")
def round_half_away() -> Callable[[np.ndarray], np.ndarray]:
    """Reference rounding with std::round semantics, as a fixture."""
    return round_half_away_from_zero
