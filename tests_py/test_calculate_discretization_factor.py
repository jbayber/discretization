from typing import Callable, List

import numpy as np
import pandas as pd
import pytest

from discretization import UnitSystem, calculate_discretization_factor, discretize
from tests_py.constants import Constants


class TestCalculateDiscretizationFactor:
    def test_metric_default(self, values_float32: np.ndarray) -> None:
        assert calculate_discretization_factor(values_float32, Constants.EPSILON) == 10

    def test_empty(self) -> None:
        assert calculate_discretization_factor(Constants.EMPTY, Constants.EPSILON) == 1

    def test_series(self, series: pd.Series) -> None:
        assert calculate_discretization_factor(series, Constants.EPSILON) == 10

    def test_dataframe_column(self, dataframe: pd.DataFrame) -> None:
        values = dataframe["x"].to_numpy(np.float32)
        assert calculate_discretization_factor(values, Constants.EPSILON) == 10

    def test_rejects_multidimensional_input(self) -> None:
        with pytest.raises(ValueError):
            calculate_discretization_factor(
                np.zeros((2, 2), dtype=np.float32), Constants.EPSILON
            )


METRIC = [*Constants.METRIC_SPELLINGS, UnitSystem.METRIC]
IMPERIAL = [*Constants.IMPERIAL_SPELLINGS, UnitSystem.IMPERIAL]
BOTH = [*METRIC, *IMPERIAL]


def _example_params(examples):
    return [pytest.param(v, e, f, id=name) for name, v, e, f, _ in examples]


class TestCalculateDiscretizationFactorMetric:
    @pytest.mark.parametrize("unit_system", METRIC, ids=str)
    @pytest.mark.parametrize(
        "values, epsilon, factor", _example_params(Constants.METRIC_EXAMPLES)
    )
    def test_examples(
        self, values: List[float], epsilon: float, factor: int, unit_system
    ) -> None:
        result = calculate_discretization_factor(
            np.array(values, dtype=np.float32), epsilon, unit_system
        )
        assert result == factor

    @pytest.mark.parametrize(
        "values, epsilon, factor", _example_params(Constants.METRIC_EXAMPLES)
    )
    def test_default_is_metric(
        self, values: List[float], epsilon: float, factor: int
    ) -> None:
        values = np.array(values, dtype=np.float32)
        assert calculate_discretization_factor(values, epsilon) == factor
        assert calculate_discretization_factor(
            values, epsilon
        ) == calculate_discretization_factor(values, epsilon, UnitSystem.METRIC)

    def test_default_differs_from_imperial(self, values_float32: np.ndarray) -> None:
        assert calculate_discretization_factor(values_float32, Constants.EPSILON) == (
            Constants.FACTOR
        )
        assert (
            calculate_discretization_factor(
                values_float32, Constants.EPSILON, "imperial"
            )
            == Constants.FACTOR_IMPERIAL
        )


class TestCalculateDiscretizationFactorImperial:
    @pytest.mark.parametrize("unit_system", IMPERIAL, ids=str)
    @pytest.mark.parametrize(
        "values, epsilon, factor", _example_params(Constants.IMPERIAL_EXAMPLES)
    )
    def test_examples(
        self, values: List[float], epsilon: float, factor: int, unit_system
    ) -> None:
        result = calculate_discretization_factor(
            np.array(values, dtype=np.float32), epsilon, unit_system
        )
        assert result == factor

    @pytest.mark.parametrize(
        "values",
        [
            Constants.VALUES,
            np.array(Constants.VALUES, dtype=np.float32),
            np.array(Constants.VALUES, dtype=np.float64),
            np.repeat(np.array(Constants.VALUES, dtype=np.float32), 2)[::2],
            pd.Series(Constants.VALUES),
        ],
        ids=["list", "float32", "float64", "strided", "series"],
    )
    def test_input_types(self, values) -> None:
        result = calculate_discretization_factor(
            values, Constants.EPSILON, UnitSystem.IMPERIAL
        )
        assert result == Constants.FACTOR_IMPERIAL


class TestCalculateDiscretizationFactorUnitSystem:
    def test_enum_members(self) -> None:
        assert UnitSystem.METRIC != UnitSystem.IMPERIAL

    def test_exported(self) -> None:
        import discretization

        assert "UnitSystem" in discretization.__all__

    @pytest.mark.parametrize("unit_system", BOTH, ids=str)
    def test_keyword_matches_positional(
        self, values_float32: np.ndarray, unit_system
    ) -> None:
        assert calculate_discretization_factor(
            values_float32, Constants.EPSILON, unit_system=unit_system
        ) == calculate_discretization_factor(
            values_float32, Constants.EPSILON, unit_system
        )

    @pytest.mark.parametrize("unit_system", Constants.INVALID_UNIT_SYSTEMS, ids=repr)
    def test_rejects_unknown_unit_system(
        self, values_float32: np.ndarray, unit_system
    ) -> None:
        with pytest.raises(ValueError):
            calculate_discretization_factor(
                values_float32, Constants.EPSILON, unit_system
            )

    @pytest.mark.parametrize("unit_system", Constants.INVALID_UNIT_SYSTEMS, ids=repr)
    def test_rejects_unknown_unit_system_keyword(
        self, values_float32: np.ndarray, unit_system
    ) -> None:
        with pytest.raises(ValueError):
            calculate_discretization_factor(
                values_float32, Constants.EPSILON, unit_system=unit_system
            )


class TestCalculateDiscretizationFactorErrors:
    @pytest.mark.parametrize("unit_system", ["metric", "imperial"])
    @pytest.mark.parametrize("epsilon", Constants.INVALID_EPSILONS, ids=repr)
    def test_invalid_epsilon(
        self, values_float32: np.ndarray, epsilon: float, unit_system: str
    ) -> None:
        with pytest.raises(ValueError):
            calculate_discretization_factor(values_float32, epsilon, unit_system)

    @pytest.mark.parametrize("unit_system", ["metric", "imperial"])
    @pytest.mark.parametrize("epsilon", Constants.INVALID_EPSILONS, ids=repr)
    def test_invalid_epsilon_on_empty_input(
        self, epsilon: float, unit_system: str
    ) -> None:
        with pytest.raises(ValueError):
            calculate_discretization_factor(Constants.EMPTY, epsilon, unit_system)

    @pytest.mark.parametrize("unit_system", ["metric", "imperial"])
    @pytest.mark.parametrize(
        "values", Constants.NON_FINITE_VALUES, ids=["nan", "inf", "-inf"]
    )
    def test_non_finite_values(self, values: List[float], unit_system: str) -> None:
        with pytest.raises(ValueError):
            calculate_discretization_factor(
                np.array(values, dtype=np.float32), Constants.EPSILON, unit_system
            )

    def test_no_candidate_fits_metric(self) -> None:
        # std::out_of_range must surface as ValueError, not IndexError.
        with pytest.raises(ValueError):
            calculate_discretization_factor(
                np.array(Constants.NO_FIT_VALUES, dtype=np.float32),
                Constants.NO_FIT_EPSILON,
                "metric",
            )

    @pytest.mark.parametrize("unit_system", ["metric", "imperial"])
    def test_int32_overflow(self, unit_system: str) -> None:
        with pytest.raises(ValueError):
            calculate_discretization_factor(
                np.array(Constants.OVERFLOW_VALUES, dtype=np.float32),
                Constants.OVERFLOW_EPSILON,
                unit_system,
            )


class TestCalculateDiscretizationFactorProperty:
    def test_reference_candidates(self) -> None:
        candidates = Constants.IMPERIAL_CANDIDATES
        assert len(candidates) == Constants.IMPERIAL_CANDIDATES_COUNT
        assert candidates[:20] == Constants.IMPERIAL_CANDIDATES_FIRST_20
        assert candidates[-1] == Constants.IMPERIAL_CANDIDATES_LAST
        assert Constants.METRIC_CANDIDATES[-1] == 10**9

    @pytest.mark.parametrize("epsilon", Constants.PROPERTY_EPSILONS)
    @pytest.mark.parametrize(
        "unit_system, candidates",
        [
            ("metric", Constants.METRIC_CANDIDATES),
            ("imperial", Constants.IMPERIAL_CANDIDATES),
        ],
        ids=["metric", "imperial"],
    )
    def test_smallest_fitting_candidate(
        self,
        property_values: np.ndarray,
        fits: Callable[[np.ndarray, int, float], bool],
        unit_system: str,
        candidates: List[int],
        epsilon: float,
    ) -> None:
        factor = calculate_discretization_factor(property_values, epsilon, unit_system)
        assert factor in candidates
        assert fits(property_values, factor, epsilon)
        index = candidates.index(factor)
        if index > 0:
            assert not fits(property_values, candidates[index - 1], epsilon)

    @pytest.mark.parametrize("epsilon", Constants.PROPERTY_EPSILONS)
    @pytest.mark.parametrize("unit_system", ["metric", "imperial"])
    def test_agrees_with_discretize(
        self, property_values: np.ndarray, unit_system: str, epsilon: float
    ) -> None:
        factor = calculate_discretization_factor(property_values, epsilon, unit_system)
        result = discretize(property_values, epsilon, unit_system)
        values = property_values.astype(np.float64)
        assert np.all(np.abs(result / factor - values) <= np.float32(epsilon))


class TestCalculateDiscretizationFactorBenchmark:
    @pytest.mark.parametrize("unit_system", ["metric", "imperial"])
    def test_large_values(
        self, benchmark, large_values: np.ndarray, unit_system: str
    ) -> None:
        factor = benchmark(
            calculate_discretization_factor,
            large_values,
            Constants.EPSILON,
            unit_system,
        )
        assert factor in (
            Constants.METRIC_CANDIDATES
            if unit_system == "metric"
            else Constants.IMPERIAL_CANDIDATES
        )
