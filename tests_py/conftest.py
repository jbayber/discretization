import tracemalloc
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


@pytest.fixture(scope="session")
def determinism_values() -> np.ndarray:
    rng = np.random.default_rng(seed=Constants.DETERMINISM_SEED)
    return rng.uniform(
        Constants.PROPERTY_LOW, Constants.PROPERTY_HIGH, Constants.DETERMINISM_SIZE
    ).astype(np.float32)


@pytest.fixture(scope="session")
def huge_values() -> np.ndarray:
    rng = np.random.default_rng(seed=Constants.HUGE_SEED)
    return rng.uniform(
        Constants.HUGE_LOW, Constants.HUGE_HIGH, Constants.HUGE_SIZE
    ).astype(np.float32)


@pytest.fixture(scope="session")
def mean_abs_error() -> Callable[[np.ndarray, int], float]:
    """Reference mean absolute error of `values` discretized with `factor`.

    float32 inputs converted to double, q = round(v * f) halfway away from
    zero, err = mean(|q / f - v|).
    """

    def _mean_abs_error(values, factor: int) -> float:
        v = np.asarray(values, dtype=np.float32).astype(np.float64)
        q = round_half_away_from_zero(v * factor)
        return float(np.mean(np.abs(q / factor - v)))

    return _mean_abs_error


def _exact_float64(seed: int, size: int) -> np.ndarray:
    """Portable float64 data in [-1000, 1000).

    Integers from the generator scaled by one IEEE multiplication, so the values
    don't depend on platform-specific floating point paths.
    """
    rng = np.random.default_rng(seed=seed)
    return rng.integers(-(2**52), 2**52, size, dtype=np.int64) * (1000.0 / 2**52)


@pytest.fixture(scope="session")
def property_values_float64() -> np.ndarray:
    return _exact_float64(Constants.PROPERTY_SEED, Constants.PROPERTY_SIZE)


@pytest.fixture(scope="session")
def determinism_values_float64() -> np.ndarray:
    return _exact_float64(Constants.DETERMINISM_SEED, Constants.DETERMINISM_SIZE)


@pytest.fixture(scope="session")
def large_values_float64() -> np.ndarray:
    rng = np.random.default_rng(seed=0)
    return rng.uniform(-1e6, 1e6, Constants.BENCHMARK_SIZE)


@pytest.fixture(scope="session")
def huge_values_float64() -> np.ndarray:
    rng = np.random.default_rng(seed=Constants.HUGE_SEED)
    return rng.uniform(Constants.HUGE_LOW, Constants.HUGE_HIGH, Constants.HUGE_SIZE)


@pytest.fixture(scope="session")
def mean_abs_error_float64() -> Callable[[np.ndarray, int], float]:
    """Reference mean absolute error of float64 `values` discretized with `factor`.

    q = round(v * f) halfway away from zero, err = mean(|q / f - v|), in double.
    """

    def _mean_abs_error(values, factor: int) -> float:
        v = np.asarray(values, dtype=np.float64)
        q = round_half_away_from_zero(v * factor)
        return float(np.mean(np.abs(q / factor - v)))

    return _mean_abs_error


@pytest.fixture(scope="session")
def peak_allocation() -> Callable[..., int]:
    """Peak traced allocation (bytes) while running `func(*args, **kwargs)`.

    NumPy reports its data buffers to tracemalloc, so a conversion copy of the
    input shows up here while a zero-copy call stays near 0.
    """

    def _peak_allocation(func, *args, **kwargs) -> int:
        tracemalloc.start()
        try:
            tracemalloc.reset_peak()
            before, _ = tracemalloc.get_traced_memory()
            func(*args, **kwargs)
            _, peak = tracemalloc.get_traced_memory()
        finally:
            tracemalloc.stop()
        return peak - before

    return _peak_allocation
