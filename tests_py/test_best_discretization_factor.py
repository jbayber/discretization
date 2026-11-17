import math
from typing import Callable, List, Optional

import numpy as np
import pandas as pd
import pytest

import discretization
from discretization import FactorResult, UnitSystem, best_discretization_factor
from tests_py.constants import Constants

ENUMS = {"metric": UnitSystem.METRIC, "imperial": UnitSystem.IMPERIAL}
CANDIDATES = {
    None: Constants.BOTH_CANDIDATES,
    "metric": Constants.METRIC_CANDIDATES,
    "imperial": Constants.IMPERIAL_CANDIDATES,
}


def _spellings(unit_system: Optional[str]) -> list:
    """Every accepted way to pass `unit_system`: None, strings and the enum."""
    if unit_system is None:
        return [None]
    return [unit_system, unit_system.upper(), unit_system.title(), ENUMS[unit_system]]


def _example_params():
    return [
        pytest.param(values, unit_system, max_factor, factor, error, result, id=name)
        for name, values, unit_system, max_factor, factor, error, result in (
            Constants.BEST_FACTOR_EXAMPLES
        )
    ]


def _call(values, unit_system, max_factor, **kwargs) -> FactorResult:
    if max_factor is None:
        return best_discretization_factor(values, unit_system, **kwargs)
    return best_discretization_factor(values, unit_system, max_factor, **kwargs)


def _assert_error(actual: float, expected: float) -> None:
    if expected == 0.0:
        assert actual == 0.0
    else:
        assert math.isclose(actual, expected, rel_tol=1e-12, abs_tol=0.0)


def _assert_identical(actual: FactorResult, expected: FactorResult) -> None:
    assert actual.factor == expected.factor
    assert actual.mean_abs_error == expected.mean_abs_error
    assert actual.unit_system == expected.unit_system


class TestBestDiscretizationFactor:
    @pytest.mark.parametrize(
        "values, unit_system, max_factor, factor, error, result", _example_params()
    )
    def test_examples(
        self,
        values: List[float],
        unit_system: Optional[str],
        max_factor: Optional[int],
        factor: int,
        error: float,
        result: str,
    ) -> None:
        for spelling in _spellings(unit_system):
            actual = _call(np.array(values, dtype=np.float32), spelling, max_factor)
            assert actual.factor == factor
            _assert_error(actual.mean_abs_error, error)
            assert actual.unit_system == ENUMS[result]

    @pytest.mark.parametrize(
        "values, unit_system, max_factor, factor, error, result", _example_params()
    )
    def test_examples_keywords(
        self,
        values: List[float],
        unit_system: Optional[str],
        max_factor: Optional[int],
        factor: int,
        error: float,
        result: str,
    ) -> None:
        kwargs = {} if max_factor is None else {"max_factor": max_factor}
        actual = best_discretization_factor(
            values=np.array(values, dtype=np.float32),
            unit_system=unit_system,
            num_threads=1,
            **kwargs,
        )
        assert actual.factor == factor
        _assert_error(actual.mean_abs_error, error)
        assert actual.unit_system == ENUMS[result]

    def test_defaults(self) -> None:
        values = np.array([0.3], dtype=np.float32)
        result = best_discretization_factor(values)
        assert result.factor == 16_777_216
        assert result.unit_system == UnitSystem.IMPERIAL
        _assert_identical(
            result,
            best_discretization_factor(values, None, Constants.INT32_MAX, 0),
        )

    @pytest.mark.parametrize("unit_system", Constants.BEST_FACTOR_SYSTEMS, ids=str)
    def test_empty(self, unit_system: Optional[str]) -> None:
        for values in (Constants.EMPTY, []):
            result = best_discretization_factor(values, unit_system)
            assert result.factor == 1
            assert result.mean_abs_error == 0.0
            assert result.unit_system == UnitSystem.METRIC

    @pytest.mark.parametrize("unit_system", Constants.BEST_FACTOR_SYSTEMS, ids=str)
    def test_all_zeros(self, unit_system: Optional[str]) -> None:
        result = best_discretization_factor(np.zeros(1000, np.float32), unit_system)
        assert result.factor == 1
        assert result.mean_abs_error == 0.0
        assert result.unit_system == UnitSystem.METRIC

    def test_max_factor_above_int32_is_clamped(self) -> None:
        values = np.array([0.3], dtype=np.float32)
        _assert_identical(
            best_discretization_factor(values, None, 10**12),
            best_discretization_factor(values),
        )
        _assert_identical(
            best_discretization_factor(values, "metric", 2**31),
            best_discretization_factor(values, "metric", Constants.INT32_MAX),
        )

    def test_rejects_multidimensional_input(self) -> None:
        with pytest.raises(ValueError):
            best_discretization_factor(np.zeros((2, 2), dtype=np.float32))


class TestBestDiscretizationFactorResult:
    @pytest.fixture
    def result(self) -> FactorResult:
        return best_discretization_factor(
            np.array([0.3125], dtype=np.float32), "imperial", 1000
        )

    def test_attribute_types(self, result: FactorResult) -> None:
        assert isinstance(result, FactorResult)
        assert type(result.factor) is int
        assert type(result.mean_abs_error) is float
        assert isinstance(result.unit_system, UnitSystem)

    def test_attribute_values(self, result: FactorResult) -> None:
        assert result.factor == 16
        assert result.mean_abs_error == 0.0
        assert result.unit_system == UnitSystem.IMPERIAL

    def test_repr(self, result: FactorResult) -> None:
        assert repr(result) == (
            "FactorResult(factor=16, mean_abs_error=0.0, "
            "unit_system=UnitSystem.IMPERIAL)"
        )

    @pytest.mark.parametrize("name", ["factor", "mean_abs_error", "unit_system"])
    def test_read_only(self, result: FactorResult, name: str) -> None:
        with pytest.raises(AttributeError):
            setattr(result, name, getattr(result, name))

    def test_no_python_constructor(self) -> None:
        with pytest.raises(TypeError):
            FactorResult()

    def test_exported(self) -> None:
        assert "best_discretization_factor" in discretization.__all__
        assert "FactorResult" in discretization.__all__


class TestBestDiscretizationFactorErrors:
    @pytest.mark.parametrize("unit_system", Constants.BEST_FACTOR_SYSTEMS, ids=str)
    @pytest.mark.parametrize(
        "values", [*Constants.NON_FINITE_VALUES, [3e9, float("nan")]], ids=repr
    )
    def test_non_finite_values(
        self, values: List[float], unit_system: Optional[str]
    ) -> None:
        with pytest.raises(ValueError):
            best_discretization_factor(np.array(values, dtype=np.float32), unit_system)

    @pytest.mark.parametrize("unit_system", Constants.BEST_FACTOR_SYSTEMS, ids=str)
    @pytest.mark.parametrize("values", Constants.BEST_FACTOR_OVERFLOW_VALUES, ids=repr)
    def test_overflow_at_factor_one(
        self, values: List[float], unit_system: Optional[str]
    ) -> None:
        # std::out_of_range must surface as ValueError, not IndexError.
        with pytest.raises(ValueError):
            best_discretization_factor(np.array(values, dtype=np.float32), unit_system)
        with pytest.raises(ValueError):
            best_discretization_factor(
                np.array(values, dtype=np.float32), unit_system, 1
            )

    @pytest.mark.parametrize(
        "max_factor", Constants.BEST_FACTOR_INVALID_MAX_FACTORS, ids=repr
    )
    @pytest.mark.parametrize("values", [Constants.VALUES, []], ids=["values", "empty"])
    def test_invalid_max_factor(self, values: List[float], max_factor: int) -> None:
        with pytest.raises(ValueError):
            best_discretization_factor(values, None, max_factor)
        with pytest.raises(ValueError):
            best_discretization_factor(values, "metric", max_factor=max_factor)

    @pytest.mark.parametrize(
        "num_threads", Constants.BEST_FACTOR_INVALID_NUM_THREADS, ids=repr
    )
    @pytest.mark.parametrize("values", [Constants.VALUES, []], ids=["values", "empty"])
    def test_invalid_num_threads(self, values: List[float], num_threads: int) -> None:
        with pytest.raises(ValueError):
            best_discretization_factor(values, num_threads=num_threads)
        with pytest.raises(ValueError):
            best_discretization_factor(values, "imperial", 100, num_threads)

    @pytest.mark.parametrize(
        "unit_system", Constants.BEST_FACTOR_INVALID_UNIT_SYSTEMS, ids=repr
    )
    def test_rejects_unknown_unit_system(
        self, values_float32: np.ndarray, unit_system
    ) -> None:
        with pytest.raises(ValueError):
            best_discretization_factor(values_float32, unit_system)
        with pytest.raises(ValueError):
            best_discretization_factor(values_float32, unit_system=unit_system)
        with pytest.raises(ValueError):
            best_discretization_factor(Constants.EMPTY, unit_system)


class TestBestDiscretizationFactorInputTypes:
    @pytest.mark.parametrize(
        "unit_system, factor, result",
        [(None, 10, "metric"), ("metric", 10, "metric"), ("imperial", 8, "imperial")],
        ids=str,
    )
    @pytest.mark.parametrize(
        "kind", ["list", "float32", "float64", "strided", "series", "dataframe"]
    )
    def test_input_types(
        self,
        dataframe: pd.DataFrame,
        series: pd.Series,
        kind: str,
        unit_system: Optional[str],
        factor: int,
        result: str,
    ) -> None:
        values = {
            "list": Constants.VALUES,
            "float32": np.array(Constants.VALUES, dtype=np.float32),
            "float64": np.array(Constants.VALUES, dtype=np.float64),
            "strided": np.repeat(np.array(Constants.VALUES, dtype=np.float32), 2)[::2],
            "series": series,
            "dataframe": dataframe["x"],
        }[kind]
        expected = best_discretization_factor(
            np.array(Constants.VALUES, dtype=np.float32), unit_system, 10
        )
        actual = best_discretization_factor(values, unit_system, 10)
        assert actual.factor == factor
        assert actual.unit_system == ENUMS[result]
        _assert_identical(actual, expected)


class TestBestDiscretizationFactorDeterminism:
    @pytest.mark.parametrize("unit_system", Constants.BEST_FACTOR_SYSTEMS, ids=str)
    def test_bit_identical_across_thread_counts(
        self, determinism_values: np.ndarray, unit_system: Optional[str]
    ) -> None:
        results = [
            best_discretization_factor(
                determinism_values, unit_system, num_threads=num_threads
            )
            for num_threads in Constants.BEST_FACTOR_THREAD_COUNTS
        ]
        for result in results[1:]:
            _assert_identical(result, results[0])

    @pytest.mark.parametrize("unit_system", Constants.BEST_FACTOR_SYSTEMS, ids=str)
    def test_below_auto_threshold(
        self, determinism_values: np.ndarray, unit_system: Optional[str]
    ) -> None:
        values = determinism_values[: Constants.AUTO_PARALLEL_THRESHOLD - 1]
        _assert_identical(
            best_discretization_factor(values, unit_system, num_threads=0),
            best_discretization_factor(values, unit_system, num_threads=1),
        )


class TestBestDiscretizationFactorProperty:
    def test_reference_candidates(self) -> None:
        candidates = Constants.BOTH_CANDIDATES
        assert len(candidates) == Constants.BOTH_CANDIDATES_COUNT
        assert candidates[:8] == [1, 2, 3, 4, 6, 8, 9, 10]
        assert candidates.count(1) == 1

    @pytest.mark.parametrize("max_factor", Constants.BEST_FACTOR_PROPERTY_BUDGETS)
    @pytest.mark.parametrize("unit_system", Constants.BEST_FACTOR_SYSTEMS, ids=str)
    def test_matches_reference(
        self,
        property_values: np.ndarray,
        mean_abs_error: Callable[[np.ndarray, int], float],
        unit_system: Optional[str],
        max_factor: int,
    ) -> None:
        result = best_discretization_factor(property_values, unit_system, max_factor)
        factor, system = Constants.BEST_FACTOR_PROPERTY_EXPECTED[
            (unit_system, max_factor)
        ]
        assert result.factor == factor
        assert result.unit_system == ENUMS[system]

        # Reference search: every candidate within the budget and the int32 cutoff.
        max_abs = float(np.max(np.abs(property_values.astype(np.float64))))
        errors = {
            f: mean_abs_error(property_values, f)
            for f in CANDIDATES[unit_system]
            if f <= max_factor and round(max_abs * f) <= Constants.INT32_MAX
        }
        assert math.isclose(
            result.mean_abs_error, errors[result.factor], rel_tol=1e-12, abs_tol=0.0
        )
        assert result.factor == min(errors, key=lambda f: (errors[f], f))
        assert all(
            result.mean_abs_error <= error * (1 + 1e-12) for error in errors.values()
        )


class TestBestDiscretizationFactorBenchmark:
    @pytest.mark.parametrize("unit_system", Constants.BEST_FACTOR_SYSTEMS, ids=str)
    def test_large_values(
        self, benchmark, large_values: np.ndarray, unit_system: Optional[str]
    ) -> None:
        result = benchmark(best_discretization_factor, large_values, unit_system)
        factor, system = Constants.BEST_FACTOR_LARGE_EXPECTED[unit_system]
        assert result.factor == factor
        assert result.unit_system == ENUMS[system]

    @pytest.mark.parametrize("num_threads", [1, 0], ids=["threads=1", "auto"])
    def test_huge_values(
        self, benchmark, huge_values: np.ndarray, num_threads: int
    ) -> None:
        result = benchmark.pedantic(
            best_discretization_factor,
            args=(huge_values, "metric"),
            kwargs={"num_threads": num_threads},
            rounds=3,
            iterations=1,
        )
        assert result.factor == Constants.HUGE_FACTOR
        assert result.unit_system == UnitSystem.METRIC
