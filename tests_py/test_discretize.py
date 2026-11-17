from typing import Callable, List

import numpy as np
import pandas as pd
import pytest

from discretization import UnitSystem, discretize
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
        result = discretize(np.array(values, dtype=np.float32), 0.5)
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


METRIC = [*Constants.METRIC_SPELLINGS, UnitSystem.METRIC]
IMPERIAL = [*Constants.IMPERIAL_SPELLINGS, UnitSystem.IMPERIAL]
BOTH = [*METRIC, *IMPERIAL]


def _example_params(examples):
    return [pytest.param(v, e, d, id=name) for name, v, e, _, d in examples]


class TestDiscretizeMetric:
    @pytest.mark.parametrize("unit_system", METRIC, ids=str)
    @pytest.mark.parametrize(
        "values, epsilon, expected", _example_params(Constants.METRIC_EXAMPLES)
    )
    def test_examples(
        self, values: List[float], epsilon: float, expected: List[int], unit_system
    ) -> None:
        result = discretize(np.array(values, dtype=np.float32), epsilon, unit_system)
        assert result.dtype == np.int32
        np.testing.assert_array_equal(result, np.array(expected, dtype=np.int32))

    @pytest.mark.parametrize(
        "values, epsilon, expected", _example_params(Constants.METRIC_EXAMPLES)
    )
    def test_default_is_metric(
        self, values: List[float], epsilon: float, expected: List[int]
    ) -> None:
        values = np.array(values, dtype=np.float32)
        np.testing.assert_array_equal(discretize(values, epsilon), expected)
        np.testing.assert_array_equal(
            discretize(values, epsilon), discretize(values, epsilon, UnitSystem.METRIC)
        )

    def test_default_differs_from_imperial(self, values_float32: np.ndarray) -> None:
        np.testing.assert_array_equal(
            discretize(values_float32, Constants.EPSILON), Constants.DISCRETIZED
        )
        np.testing.assert_array_equal(
            discretize(values_float32, Constants.EPSILON, "imperial"),
            Constants.DISCRETIZED_IMPERIAL,
        )


class TestDiscretizeImperial:
    @pytest.mark.parametrize("unit_system", IMPERIAL, ids=str)
    @pytest.mark.parametrize(
        "values, epsilon, expected", _example_params(Constants.IMPERIAL_EXAMPLES)
    )
    def test_examples(
        self, values: List[float], epsilon: float, expected: List[int], unit_system
    ) -> None:
        result = discretize(np.array(values, dtype=np.float32), epsilon, unit_system)
        assert result.dtype == np.int32
        np.testing.assert_array_equal(result, np.array(expected, dtype=np.int32))


class TestDiscretizeInputTypesImperial:
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
            discretize(values, Constants.EPSILON, UnitSystem.IMPERIAL),
            Constants.DISCRETIZED_IMPERIAL,
        )

    def test_series(self, series: pd.Series) -> None:
        np.testing.assert_array_equal(
            discretize(series, Constants.EPSILON, "imperial"),
            Constants.DISCRETIZED_IMPERIAL,
        )

    def test_dataframe_column(self, dataframe: pd.DataFrame) -> None:
        values = dataframe["x"].to_numpy(np.float32)
        np.testing.assert_array_equal(
            discretize(values, Constants.EPSILON, unit_system="imperial"),
            Constants.DISCRETIZED_IMPERIAL,
        )


class TestDiscretizeUnitSystem:
    @pytest.mark.parametrize("unit_system", BOTH, ids=str)
    def test_keyword_matches_positional(
        self, values_float32: np.ndarray, unit_system
    ) -> None:
        np.testing.assert_array_equal(
            discretize(values_float32, Constants.EPSILON, unit_system=unit_system),
            discretize(values_float32, Constants.EPSILON, unit_system),
        )

    @pytest.mark.parametrize("unit_system", Constants.INVALID_UNIT_SYSTEMS, ids=repr)
    def test_rejects_unknown_unit_system(
        self, values_float32: np.ndarray, unit_system
    ) -> None:
        with pytest.raises(ValueError):
            discretize(values_float32, Constants.EPSILON, unit_system)

    @pytest.mark.parametrize("unit_system", Constants.INVALID_UNIT_SYSTEMS, ids=repr)
    def test_rejects_unknown_unit_system_keyword(
        self, values_float32: np.ndarray, unit_system
    ) -> None:
        with pytest.raises(ValueError):
            discretize(values_float32, Constants.EPSILON, unit_system=unit_system)


class TestDiscretizeErrors:
    @pytest.mark.parametrize("unit_system", ["metric", "imperial"])
    @pytest.mark.parametrize("epsilon", Constants.INVALID_EPSILONS, ids=repr)
    def test_invalid_epsilon(
        self, values_float32: np.ndarray, epsilon: float, unit_system: str
    ) -> None:
        with pytest.raises(ValueError):
            discretize(values_float32, epsilon, unit_system)

    @pytest.mark.parametrize("unit_system", ["metric", "imperial"])
    @pytest.mark.parametrize("epsilon", Constants.INVALID_EPSILONS, ids=repr)
    def test_invalid_epsilon_on_empty_input(
        self, epsilon: float, unit_system: str
    ) -> None:
        with pytest.raises(ValueError):
            discretize(Constants.EMPTY, epsilon, unit_system)

    @pytest.mark.parametrize("unit_system", ["metric", "imperial"])
    @pytest.mark.parametrize(
        "values", Constants.NON_FINITE_VALUES, ids=["nan", "inf", "-inf"]
    )
    def test_non_finite_values(self, values: List[float], unit_system: str) -> None:
        with pytest.raises(ValueError):
            discretize(
                np.array(values, dtype=np.float32), Constants.EPSILON, unit_system
            )

    def test_no_candidate_fits_metric(self) -> None:
        # std::out_of_range must surface as ValueError, not IndexError.
        with pytest.raises(ValueError):
            discretize(
                np.array(Constants.NO_FIT_VALUES, dtype=np.float32),
                Constants.NO_FIT_EPSILON,
                "metric",
            )

    @pytest.mark.parametrize("unit_system", ["metric", "imperial"])
    def test_int32_overflow(self, unit_system: str) -> None:
        with pytest.raises(ValueError):
            discretize(
                np.array(Constants.OVERFLOW_VALUES, dtype=np.float32),
                Constants.OVERFLOW_EPSILON,
                unit_system,
            )


class TestDiscretizeProperty:
    @pytest.mark.parametrize("epsilon", Constants.PROPERTY_EPSILONS)
    @pytest.mark.parametrize(
        "unit_system, candidates",
        [
            ("metric", Constants.METRIC_CANDIDATES),
            ("imperial", Constants.IMPERIAL_CANDIDATES),
        ],
        ids=["metric", "imperial"],
    )
    def test_within_epsilon_with_smallest_factor(
        self,
        property_values: np.ndarray,
        fits: Callable[[np.ndarray, int, float], bool],
        round_half_away: Callable[[np.ndarray], np.ndarray],
        unit_system: str,
        candidates: List[int],
        epsilon: float,
    ) -> None:
        result = discretize(property_values, epsilon, unit_system)
        assert result.dtype == np.int32
        assert result.size == property_values.size

        # Recover the factor from the reference search and check the result
        # is exactly round(v * f), within epsilon, with f the smallest fit.
        factor = next(f for f in candidates if fits(property_values, f, epsilon))
        values = property_values.astype(np.float64)
        expected = round_half_away(values * factor)
        np.testing.assert_array_equal(result, expected.astype(np.int32))
        assert np.all(np.abs(result / factor - values) <= np.float32(epsilon))


class TestDiscretizeImperialBenchmark:
    def test_float32_no_copy(self, benchmark, large_values: np.ndarray) -> None:
        result = benchmark(discretize, large_values, Constants.EPSILON, "imperial")
        assert result.size == large_values.size
