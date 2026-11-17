"""Tests of best_discretization_factor on float64 input and the dtype dispatch.

float32 input keeps the float32 path; everything else (float64, lists, float16,
integers, float64 Series) runs in float64 without a lossy float32 conversion.
See specs/best-factor-float64/spec.md.
"""

import math
from typing import Callable, List, Optional

import numpy as np
import pandas as pd
import pytest

from discretization import FactorResult, UnitSystem, best_discretization_factor
from tests_py.constants import Constants

ENUMS = {"metric": UnitSystem.METRIC, "imperial": UnitSystem.IMPERIAL}
CANDIDATES = {
    None: Constants.BOTH_CANDIDATES,
    "metric": Constants.METRIC_CANDIDATES,
    "imperial": Constants.IMPERIAL_CANDIDATES,
}
MIXED = [0.1, 2.5, -3.7]


def _spellings(unit_system: Optional[str]) -> list:
    """Every accepted way to pass `unit_system`: None, strings and the enum."""
    if unit_system is None:
        return [None]
    return [unit_system, unit_system.upper(), unit_system.title(), ENUMS[unit_system]]


def _example_params():
    return [
        pytest.param(values, unit_system, max_factor, factor, error, result, id=name)
        for name, values, unit_system, max_factor, factor, error, result in (
            Constants.BEST_FACTOR_FLOAT64_EXAMPLES
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


def _assert_result(
    actual: FactorResult, factor: int, error: float, result: str
) -> None:
    assert actual.factor == factor
    _assert_error(actual.mean_abs_error, error)
    assert actual.unit_system == ENUMS[result]


def _assert_identical(actual: FactorResult, expected: FactorResult) -> None:
    assert actual.factor == expected.factor
    assert actual.mean_abs_error == expected.mean_abs_error
    assert actual.unit_system == expected.unit_system


class TestBestDiscretizationFactorFloat64:
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
            actual = _call(np.array(values, dtype=np.float64), spelling, max_factor)
            _assert_result(actual, factor, error, result)

    @pytest.mark.parametrize(
        "values, unit_system, max_factor, factor, error, result", _example_params()
    )
    def test_examples_list(
        self,
        values: List[float],
        unit_system: Optional[str],
        max_factor: Optional[int],
        factor: int,
        error: float,
        result: str,
    ) -> None:
        _assert_result(
            _call(list(values), unit_system, max_factor), factor, error, result
        )

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
            values=np.array(values, dtype=np.float64),
            unit_system=unit_system,
            num_threads=1,
            **kwargs,
        )
        _assert_result(actual, factor, error, result)

    def test_defaults(self) -> None:
        values = np.array([0.3], dtype=np.float64)
        result = best_discretization_factor(values)
        _assert_result(result, 10, 0.0, "metric")
        _assert_identical(
            result,
            best_discretization_factor(values, None, Constants.INT32_MAX, 0),
        )

    def test_list_0_3_is_float64(self) -> None:
        # The README example: a list is float64 data, so [0.3] gives 10 (not 2^24).
        _assert_result(best_discretization_factor([0.3]), 10, 0.0, "metric")

    def test_single_value(self) -> None:
        _assert_result(
            best_discretization_factor(np.array([0.1]), "metric"), 10, 0.0, "metric"
        )

    @pytest.mark.parametrize("unit_system", Constants.BEST_FACTOR_SYSTEMS, ids=str)
    def test_all_zeros(self, unit_system: Optional[str]) -> None:
        result = best_discretization_factor(np.zeros(1000, np.float64), unit_system)
        _assert_result(result, 1, 0.0, "metric")

    @pytest.mark.parametrize("value", [5e-324, -5e-324], ids=repr)
    @pytest.mark.parametrize("unit_system", Constants.BEST_FACTOR_SYSTEMS, ids=str)
    def test_subnormal_error_is_exact(
        self, unit_system: Optional[str], value: float
    ) -> None:
        result = best_discretization_factor(np.array([value]), unit_system)
        assert result.factor == 1
        assert result.mean_abs_error == 5e-324
        assert result.unit_system == UnitSystem.METRIC

    def test_max_factor_above_int32_is_clamped(self) -> None:
        values = np.array([0.3], dtype=np.float64)
        _assert_identical(
            best_discretization_factor(values, None, 10**12),
            best_discretization_factor(values),
        )


class TestBestDiscretizationFactorDtypeDispatch:
    def test_float32_ndarray(self) -> None:
        result = best_discretization_factor(np.array(MIXED, dtype=np.float32), "metric")
        _assert_result(result, 100_000_000, 1.2688000557610952e-09, "metric")

    def test_float32_strided_view(self) -> None:
        contiguous = np.array(MIXED, dtype=np.float32)
        strided = np.repeat(contiguous, 2)[::2]
        assert not strided.flags["C_CONTIGUOUS"]
        _assert_identical(
            best_discretization_factor(strided, "metric"),
            best_discretization_factor(contiguous, "metric"),
        )
        assert best_discretization_factor(strided, "metric").factor == 100_000_000

    def test_float32_series(self) -> None:
        series = pd.Series(MIXED, dtype=np.float32)
        _assert_identical(
            best_discretization_factor(series, "metric"),
            best_discretization_factor(np.array(MIXED, dtype=np.float32), "metric"),
        )
        assert best_discretization_factor(series, "metric").factor == 100_000_000

    def test_float64_ndarray(self) -> None:
        result = best_discretization_factor(np.array(MIXED, dtype=np.float64), "metric")
        _assert_result(result, 10, 0.0, "metric")

    @pytest.mark.parametrize("kind", ["list", "series", "dataframe", "strided"])
    def test_float64_kinds(self, kind: str) -> None:
        contiguous = np.array(MIXED, dtype=np.float64)
        values = {
            "list": list(MIXED),
            "series": pd.Series(MIXED, dtype=np.float64),
            "dataframe": pd.DataFrame({"x": MIXED})["x"],
            "strided": np.repeat(contiguous, 2)[::2],
        }[kind]
        result = best_discretization_factor(values, "metric")
        _assert_result(result, 10, 0.0, "metric")
        _assert_identical(result, best_discretization_factor(contiguous, "metric"))

    def test_float16_is_widened_exactly(self) -> None:
        values = np.array([0.1], dtype=np.float16)
        assert float(values.astype(np.float64)[0]) == Constants.FLOAT16_TENTH
        result = best_discretization_factor(values)
        _assert_result(result, 8192, 0.0, "imperial")
        _assert_identical(
            result, best_discretization_factor(np.array([Constants.FLOAT16_TENTH]))
        )

    @pytest.mark.parametrize("unit_system", Constants.BEST_FACTOR_SYSTEMS, ids=str)
    @pytest.mark.parametrize("dtype", [np.int32, np.int64], ids=["int32", "int64"])
    def test_integers(self, dtype, unit_system: Optional[str]) -> None:
        result = best_discretization_factor(
            np.array([3, -7, 0], dtype=dtype), unit_system
        )
        _assert_result(result, 1, 0.0, "metric")

    def test_int64_above_2_24(self) -> None:
        # 2^24 + 1 isn't a float32 (it would become 16777216) but is an exact double.
        values = np.array([16_777_217], dtype=np.int64)
        for unit_system in Constants.BEST_FACTOR_SYSTEMS:
            _assert_result(
                best_discretization_factor(values, unit_system), 1, 0.0, "metric"
            )

    def test_float64_half_above_2_24(self) -> None:
        values = [16_777_217.5]
        _assert_result(
            best_discretization_factor(np.array(values, dtype=np.float64)),
            2,
            0.0,
            "imperial",
        )
        # float32 rounds it to 16777218, an integer.
        _assert_result(
            best_discretization_factor(np.array(values, dtype=np.float32)),
            1,
            0.0,
            "metric",
        )

    def test_point_three_float32_vs_float64(self) -> None:
        _assert_result(
            best_discretization_factor(np.array([0.3], dtype=np.float32)),
            16_777_216,
            0.0,
            "imperial",
        )
        _assert_result(
            best_discretization_factor(np.array([0.3], dtype=np.float64)),
            10,
            0.0,
            "metric",
        )


class TestBestDiscretizationFactorCopies:
    """Contiguous float32 and float64 are passed to C++ without a copy; other
    layouts and dtypes are converted once (float16 and integers to float64)."""

    N = Constants.ALLOCATION_SIZE

    @staticmethod
    def _run(values: np.ndarray) -> FactorResult:
        return best_discretization_factor(values, "metric", 10, 1)

    def _values(self, dtype) -> np.ndarray:
        rng = np.random.default_rng(seed=3)
        return rng.integers(-1000, 1000, self.N).astype(dtype)

    @pytest.mark.parametrize("dtype", [np.float32, np.float64], ids=str)
    def test_contiguous_is_zero_copy(
        self, peak_allocation: Callable[..., int], dtype
    ) -> None:
        values = self._values(dtype)
        assert values.flags["C_CONTIGUOUS"]
        assert peak_allocation(self._run, values) < 1_000_000

    def test_strided_float64_is_copied_once_as_float64(
        self, peak_allocation: Callable[..., int]
    ) -> None:
        values = np.repeat(self._values(np.float64), 2)[::2]
        assert 8 * self.N <= peak_allocation(self._run, values) < 16 * self.N

    def test_strided_float32_is_copied_once_as_float32(
        self, peak_allocation: Callable[..., int]
    ) -> None:
        values = np.repeat(self._values(np.float32), 2)[::2]
        assert 4 * self.N <= peak_allocation(self._run, values) < 8 * self.N

    @pytest.mark.parametrize("dtype", [np.float16, np.int32], ids=str)
    def test_other_dtypes_are_widened_once_to_float64(
        self, peak_allocation: Callable[..., int], dtype
    ) -> None:
        values = self._values(dtype)
        assert 8 * self.N <= peak_allocation(self._run, values) < 16 * self.N


class TestBestDiscretizationFactorFloat64Errors:
    @pytest.mark.parametrize("unit_system", Constants.BEST_FACTOR_SYSTEMS, ids=str)
    @pytest.mark.parametrize(
        "values", Constants.BEST_FACTOR_FLOAT64_NON_FINITE_VALUES, ids=repr
    )
    def test_non_finite_values(
        self, values: List[float], unit_system: Optional[str]
    ) -> None:
        with pytest.raises(ValueError):
            best_discretization_factor(np.array(values, dtype=np.float64), unit_system)
        with pytest.raises(ValueError):
            best_discretization_factor(list(values), unit_system)

    @pytest.mark.parametrize("unit_system", Constants.BEST_FACTOR_SYSTEMS, ids=str)
    @pytest.mark.parametrize(
        "values", Constants.BEST_FACTOR_FLOAT64_OVERFLOW_VALUES, ids=repr
    )
    def test_overflow_at_factor_one(
        self, values: List[float], unit_system: Optional[str]
    ) -> None:
        # std::out_of_range must surface as ValueError, not IndexError.
        array = np.array(values, dtype=np.float64)
        with pytest.raises(ValueError):
            best_discretization_factor(array, unit_system)
        with pytest.raises(ValueError):
            best_discretization_factor(array, unit_system, 1)

    @pytest.mark.parametrize(
        "max_factor", Constants.BEST_FACTOR_INVALID_MAX_FACTORS, ids=repr
    )
    @pytest.mark.parametrize(
        "values",
        [np.array(Constants.VALUES, dtype=np.float64), np.array([], dtype=np.float64)],
        ids=["values", "empty"],
    )
    def test_invalid_max_factor(self, values: np.ndarray, max_factor: int) -> None:
        with pytest.raises(ValueError):
            best_discretization_factor(values, None, max_factor)
        with pytest.raises(ValueError):
            best_discretization_factor(values, "metric", max_factor=max_factor)

    @pytest.mark.parametrize(
        "num_threads", Constants.BEST_FACTOR_INVALID_NUM_THREADS, ids=repr
    )
    @pytest.mark.parametrize(
        "values",
        [np.array(Constants.VALUES, dtype=np.float64), np.array([], dtype=np.float64)],
        ids=["values", "empty"],
    )
    def test_invalid_num_threads(self, values: np.ndarray, num_threads: int) -> None:
        with pytest.raises(ValueError):
            best_discretization_factor(values, num_threads=num_threads)
        with pytest.raises(ValueError):
            best_discretization_factor(values, "imperial", 100, num_threads)

    @pytest.mark.parametrize("dtype", [np.float64, np.int64], ids=str)
    def test_rejects_multidimensional_input(self, dtype) -> None:
        with pytest.raises(ValueError):
            best_discretization_factor(np.zeros((2, 2), dtype=dtype))

    @pytest.mark.parametrize("values", [0.5, None], ids=repr)
    def test_rejects_scalars_and_none(self, values) -> None:
        with pytest.raises(ValueError):
            best_discretization_factor(values)

    def test_rejects_non_numeric(self) -> None:
        with pytest.raises((TypeError, ValueError)):
            best_discretization_factor(["a", "b"])

    @pytest.mark.parametrize(
        "unit_system", Constants.BEST_FACTOR_INVALID_UNIT_SYSTEMS, ids=repr
    )
    def test_rejects_unknown_unit_system(self, unit_system) -> None:
        with pytest.raises(ValueError):
            best_discretization_factor(np.array(MIXED, dtype=np.float64), unit_system)
        with pytest.raises(ValueError):
            best_discretization_factor(list(MIXED), unit_system=unit_system)

    @pytest.mark.parametrize("unit_system", Constants.BEST_FACTOR_SYSTEMS, ids=str)
    def test_empty(self, unit_system: Optional[str]) -> None:
        for values in (np.array([], dtype=np.float64), []):
            result = best_discretization_factor(values, unit_system)
            assert result.factor == 1
            assert result.mean_abs_error == 0.0
            assert result.unit_system == UnitSystem.METRIC


class TestBestDiscretizationFactorFloat64Determinism:
    @pytest.mark.parametrize("unit_system", Constants.BEST_FACTOR_SYSTEMS, ids=str)
    def test_bit_identical_across_thread_counts(
        self, determinism_values_float64: np.ndarray, unit_system: Optional[str]
    ) -> None:
        assert determinism_values_float64.dtype == np.float64
        results = [
            best_discretization_factor(
                determinism_values_float64, unit_system, num_threads=num_threads
            )
            for num_threads in Constants.BEST_FACTOR_THREAD_COUNTS
        ]
        for result in results[1:]:
            _assert_identical(result, results[0])

    @pytest.mark.parametrize("unit_system", Constants.BEST_FACTOR_SYSTEMS, ids=str)
    def test_below_auto_threshold(
        self, determinism_values_float64: np.ndarray, unit_system: Optional[str]
    ) -> None:
        values = determinism_values_float64[: Constants.AUTO_PARALLEL_THRESHOLD - 1]
        _assert_identical(
            best_discretization_factor(values, unit_system, num_threads=0),
            best_discretization_factor(values, unit_system, num_threads=1),
        )


class TestBestDiscretizationFactorFloat64Property:
    @pytest.mark.parametrize("max_factor", Constants.BEST_FACTOR_PROPERTY_BUDGETS)
    @pytest.mark.parametrize("unit_system", Constants.BEST_FACTOR_SYSTEMS, ids=str)
    def test_matches_reference(
        self,
        property_values_float64: np.ndarray,
        mean_abs_error_float64: Callable[[np.ndarray, int], float],
        unit_system: Optional[str],
        max_factor: int,
    ) -> None:
        values = property_values_float64
        result = best_discretization_factor(values, unit_system, max_factor)
        expected = Constants.BEST_FACTOR_FLOAT64_PROPERTY_EXPECTED[
            (unit_system, max_factor)
        ]
        if expected is not None:
            factor, system = expected
            assert result.factor == factor
            assert result.unit_system == ENUMS[system]

        # Reference search: every candidate within the budget and the int32 cutoff.
        max_abs = float(np.max(np.abs(values)))
        errors = {
            f: mean_abs_error_float64(values, f)
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
        reference_system = (
            "metric" if result.factor in CANDIDATES["metric"] else ("imperial")
        )
        assert result.unit_system == ENUMS[reference_system]


class TestBestDiscretizationFactorFloat64Benchmark:
    @pytest.mark.parametrize("unit_system", Constants.BEST_FACTOR_SYSTEMS, ids=str)
    def test_large_values_float64(
        self, benchmark, large_values_float64: np.ndarray, unit_system: Optional[str]
    ) -> None:
        result = benchmark(
            best_discretization_factor, large_values_float64, unit_system
        )
        factor, system = Constants.BEST_FACTOR_LARGE_FLOAT64_EXPECTED[unit_system]
        assert result.factor == factor
        assert result.unit_system == ENUMS[system]

    @pytest.mark.parametrize("num_threads", [1, 0], ids=["threads=1", "auto"])
    def test_huge_values_float64(
        self, benchmark, huge_values_float64: np.ndarray, num_threads: int
    ) -> None:
        result = benchmark.pedantic(
            best_discretization_factor,
            args=(huge_values_float64, "metric"),
            kwargs={"num_threads": num_threads},
            rounds=3,
            iterations=1,
        )
        assert result.factor == Constants.HUGE_FACTOR
        assert result.unit_system == UnitSystem.METRIC
