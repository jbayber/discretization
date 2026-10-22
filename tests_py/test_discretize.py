from typing import List

import numpy as np
import pandas as pd
import pytest

from discretization import discretize
from tests_py.constants import Constants


class TestDiscretize:
    def test_returns_int32(self, values_float32: np.ndarray) -> None:
        assert discretize(values_float32, Constants.EPSILON).dtype == np.int32

    @pytest.mark.parametrize(
        "values, expected",
        [
            ([0.0, 1.2, 1.7, -1.2, -1.7], [0, 1, 2, -1, -2]),
            ([0.5, 1.5, 2.5, -0.5, -2.5], [1, 2, 3, -1, -3]),
        ],
        ids=["nearest", "halfway-away-from-zero"],
    )
    def test_rounding(self, values: List[float], expected: List[int]) -> None:
        result = discretize(np.array(values, dtype=np.float32), Constants.EPSILON)
        np.testing.assert_array_equal(result, expected)

    def test_empty(self) -> None:
        assert discretize(Constants.EMPTY, Constants.EPSILON).size == 0

    def test_rejects_multidimensional_input(self) -> None:
        with pytest.raises(ValueError):
            discretize(np.zeros((2, 2), dtype=np.float32), Constants.EPSILON)


class TestDiscretizeInputTypes:
    @pytest.mark.parametrize(
        "values",
        [
            Constants.VALUES,
            np.array(Constants.VALUES, dtype=np.float32),
            np.array(Constants.VALUES, dtype=np.float64),
            np.repeat(np.array(Constants.VALUES, dtype=np.float32), 2)[::2],
        ],
        ids=["list", "float32", "float64", "strided"],
    )
    def test_array_likes(self, values) -> None:
        np.testing.assert_array_equal(
            discretize(values, Constants.EPSILON), Constants.DISCRETIZED
        )

    def test_series(self, series: pd.Series) -> None:
        np.testing.assert_array_equal(
            discretize(series, Constants.EPSILON), Constants.DISCRETIZED
        )

    def test_dataframe_column(self, dataframe: pd.DataFrame) -> None:
        values = dataframe["x"].to_numpy(np.float32)
        np.testing.assert_array_equal(
            discretize(values, Constants.EPSILON), Constants.DISCRETIZED
        )


class TestDiscretizeBenchmark:
    def test_float32_no_copy(self, benchmark, large_values: np.ndarray) -> None:
        result = benchmark(discretize, large_values, Constants.EPSILON)
        assert result.size == large_values.size

    def test_float64_converted(self, benchmark, large_values: np.ndarray) -> None:
        values = large_values.astype(np.float64)
        result = benchmark(discretize, values, Constants.EPSILON)
        assert result.size == values.size
