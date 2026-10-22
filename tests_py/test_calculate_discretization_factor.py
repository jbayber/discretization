import numpy as np
import pandas as pd
import pytest

from discretization import calculate_discretization_factor
from tests_py.constants import Constants


class TestCalculateDiscretizationFactor:
    def test_returns_one(self, values_float32: np.ndarray) -> None:
        assert calculate_discretization_factor(values_float32, Constants.EPSILON) == 1

    def test_empty(self) -> None:
        assert calculate_discretization_factor(Constants.EMPTY, Constants.EPSILON) == 1

    def test_series(self, series: pd.Series) -> None:
        assert calculate_discretization_factor(series, Constants.EPSILON) == 1

    def test_dataframe_column(self, dataframe: pd.DataFrame) -> None:
        values = dataframe["x"].to_numpy(np.float32)
        assert calculate_discretization_factor(values, Constants.EPSILON) == 1

    def test_rejects_multidimensional_input(self) -> None:
        with pytest.raises(ValueError):
            calculate_discretization_factor(
                np.zeros((2, 2), dtype=np.float32), Constants.EPSILON
            )
