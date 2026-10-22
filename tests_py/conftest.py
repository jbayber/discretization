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
