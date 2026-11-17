# Plan: Unit-system discretization factors

Written by the `planner` subagent. Orchestrator decisions on its risks are listed at the end.

## Summary

This replaces the placeholder `calculate_discretization_factor`, which always returns 1. The new version picks the smallest factor that fits from a static, compile-time list of candidates: metric is `10^0..10^9`, imperial is the 328 values of `2^a·3^b ≤ INT32_MAX`. The last imperial candidate is 2066242608. `discretize` returns `round(v·f)` (halfway cases away from zero) as `int32`. Both functions get a trailing `UnitSystem unit_system = UnitSystem::Metric` parameter.

On the Python side, `unit_system` is a case-insensitive string or the bound `UnitSystem` enum, and `std::out_of_range` maps to `ValueError`. The existing binding rules stay: zero copy for contiguous float32, 1-D only, and the GIL released during the C++ call.

## API changes

```cpp
enum class UnitSystem { Metric, Imperial };

int calculate_discretization_factor(const std::vector<float> &values, float epsilon,
                                    UnitSystem unit_system = UnitSystem::Metric);
int calculate_discretization_factor(const float *values, std::size_t size, float epsilon,
                                    UnitSystem unit_system = UnitSystem::Metric);
std::vector<int> discretize(const std::vector<float> &values, float epsilon,
                            UnitSystem unit_system = UnitSystem::Metric);
void discretize(const float *values, std::size_t size, float epsilon, int *out,
                UnitSystem unit_system = UnitSystem::Metric);
```

- `std::invalid_argument` is thrown when epsilon is NaN or `≤ 0`, when any value is NaN or ±inf, or when the `UnitSystem` value is not one of the two enumerators.
- `std::out_of_range` is thrown when no candidate fits, or when `round(v·f)` overflows `int32`.

Python:

```python
class UnitSystem: METRIC, IMPERIAL           # py::enum_, no export_values
calculate_discretization_factor(values, epsilon, unit_system="metric") -> int
discretize(values, epsilon, unit_system="metric") -> np.ndarray[int32]
```

`unit_system` accepts a `str` (compared after `.lower()`) or a `UnitSystem`. Anything else raises `ValueError`, including `"cubits"`, `""`, `" metric"`, `0` and `None`.

## Files to change

| File | Change |
|---|---|
| `cpp/include/discretization/discretization.hpp` | `UnitSystem` enum, new signatures with defaults, doc comments |
| `cpp/src/discretization.cpp` | constexpr candidate tables, validation, factor search, `discretize` |
| `python/src/bindings.cpp` | `py::enum_`, `unit_system` parser, `out_of_range` → `ValueError` |
| `python/src/discretization/__init__.py` | export `UnitSystem` |
| `tests_cpp/test_discretization.cpp` | authorized regression edits only |
| `tests_cpp/test_unit_system_factors.cpp` (new) | example table, default-argument tests |
| `tests_cpp/test_errors.cpp` (new) | `invalid_argument` and `out_of_range` cases |
| `tests_cpp/test_property.cpp` (new) | randomized property test |
| `tests_py/constants.py`, `conftest.py` | `DISCRETIZED` change, example tables, candidates, `property_values` and `fits` fixtures |
| `tests_py/test_calculate_discretization_factor.py`, `test_discretize.py` | regression edits and new test classes |
| `README.md` | API section |

No CMake changes and no new dependencies.

## Shared example table

| id | values | eps | system | f | D |
|---|---|---|---|---|---|
| m-mixed | {0.1, 2.5, -3.7} | 0.01 | Metric | 10 | {1, 25, -37} |
| m-1234-fine | {1.234} | 0.001 | Metric | 1000 | {1234} |
| m-1234-coarse | {1.234} | 0.005 | Metric | 100 | {123} |
| m-ints | {3, -7, 0} | 0.01 | Metric | 1 | {3, -7, 0} |
| m-third | {1/3} | 1e-6 | Metric | 1000000 | {333333} |
| m-values | {0.4, 1.6, -2.5} | 0.01 | Metric | 10 | {4, 16, -25} |
| m-nearest | {0, 1.2, 1.7, -1.2, -1.7} | 0.5 | Metric | 1 | {0, 1, 2, -1, -2} |
| m-halfway | {0.5, 1.5, 2.5, -0.5, -2.5} | 0.5 | Metric | 1 | {1, 2, 3, -1, -3} |
| m-empty | {} | 0.01 | Metric | 1 | {} |
| i-5/16 | {0.3125} | 1e-6 | Imperial | 16 | {5} |
| i-neg-5/16 | {-0.3125} | 1e-6 | Imperial | 16 | {-5} |
| i-third | {1/3} | 1e-6 | Imperial | 3 | {1} |
| i-7/12 | {7/12} | 1e-6 | Imperial | 12 | {7} |
| i-2.25 | {2.25, 0.5} | 1e-6 | Imperial | 4 | {9, 2} |
| i-third-quarter | {1/3, 0.25} | 1e-6 | Imperial | 12 | {4, 3} |
| i-sixteenths | {0.5, 0.25, 0.3125} | 1e-6 | Imperial | 16 | {8, 4, 5} |
| i-mixed | {0.1, 2.5, -3.7} | 0.01 | Imperial | 48 | {5, 120, -178} |
| i-values | {0.4, 1.6, -2.5} | 0.01 | Imperial | 32 | {13, 51, -80} |
| i-ints / i-nearest / i-halfway / i-empty | as metric | | Imperial | 1 | as metric |

## Test plan

### C++

- **`test_discretization.cpp` regression edits.**
  - `VectorReturnsOne` → `VectorMetric`, expecting 10.
  - `VectorRoundsToNearest` and `HalfwayRoundsAwayFromZero`: epsilon becomes 0.5.
  - All other tests stay untouched.
- **`test_unit_system_factors.cpp`.**
  - Parametrized `UnitSystemExamples` (Metric/Imperial instantiations): factor and `discretize`, through the vector and raw versions.
  - `UnitSystemDefault.FactorDefaultsToMetric` / `DiscretizeDefaultsToMetric`, including the discriminating case `{0.4, 1.6, -2.5}` → 10 / {4, 16, -25}.
- **`test_errors.cpp`.** Each case runs both functions × vector/raw × both systems.
  - `EpsilonZero`, `EpsilonNegative`, `EpsilonNaN`, `EpsilonInvalidOnEmptyInput` → `invalid_argument`.
  - `NaNValue`, `PosInfValue`, `NegInfValue` → `invalid_argument`.
  - `NoCandidateFitsMetric` (`{0.1}`, 1e-12) → `out_of_range`.
  - `Int32Overflow` (`{3e9}`, 0.5) → `out_of_range`.
- **`test_property.cpp`.** `mt19937(42)`, 10k values in [−1000, 1000], eps {0.1, 0.01, 0.001} × both systems. Checks:
  - `f` is a candidate;
  - `f` fits;
  - `discretize` is within eps;
  - the previous candidate doesn't fit;
  - the raw version matches the vector version;
  - the imperial list has 328 entries.

### Python (MAGE style)

- **`constants.py`.**
  - `DISCRETIZED = [4, 16, -25]`, `DISCRETIZED_IMPERIAL = [13, 51, -80]`.
  - Example tables, spellings, candidate lists, property-test parameters (seed 42, 10k values).
- **`conftest.py`.**
  - `property_values` fixture.
  - `fits` reference fixture: half-away-from-zero rounding (not `np.round`), with eps cast through `np.float32`.
- **`test_calculate_discretization_factor.py`.**
  - Regression: `test_returns_one` → `test_metric_default` (10); `test_series` / `test_dataframe_column` → 10.
  - New classes `...Metric`, `...Imperial`, `...UnitSystem`, `...Errors`, `...Property` and `...Benchmark` (both systems).
- **`test_discretize.py`.**
  - Regression: `test_rounding` uses eps 0.5.
  - The input-type tests are parametrized over metric (no argument, `DISCRETIZED`) and imperial (`DISCRETIZED_IMPERIAL`).
  - New classes `...Metric`, `...Imperial`, `...UnitSystem`, `...Errors` and `...Property`, plus an imperial benchmark.

## Implementation steps

1. **Header.** Add the enum and signatures, and document "fits" and the exceptions.
2. **Candidate tables.** `constexpr std::array<int, 10>` for metric and `std::array<int, 328>` for imperial. The imperial table is built by a constexpr function with a hand-written insertion sort; don't rely on constexpr `std::sort` in Apple libc++. Add `static_assert`s: front is 1, back is 2066242608, `[19]` is 96, and the list is sorted. A `candidates_for(UnitSystem)` switch throws `invalid_argument` for an unknown enum value.
3. **`validate`.** `!(epsilon > 0)` → `invalid_argument`. A full `isfinite` pass → `invalid_argument`. Validation runs before the empty-input shortcut.
4. **`round_checked`.** Use `std::round` on a double. A result outside `[INT32_MIN, INT32_MAX]` throws `out_of_range`. Don't use `lround`, because `long` is 32 bits on Windows.
5. **Factor search.**
   - For each candidate, scan the values: `q = round_checked(v·f)` comes first (so an overflow throws immediately), then the check `|q/f − v| > eps` breaks out of the scan for that candidate.
   - Return the first candidate that fits, or throw `out_of_range`.
   - Compute the error exactly as `|q/f − v|`.
6. **Vector versions** delegate to the raw versions.
7. **`discretize`.** Call the raw factor function, then `out[i] = (int)std::round(double(v)·f)`. Don't touch `out` when size is 0.
8. **Bindings.**
   - `parse_unit_system(py::object)` takes a string (lowercased) or a registered enum; anything else raises `py::value_error`.
   - Catch `std::out_of_range` around the GIL-released call and rethrow `py::value_error`.
   - Register `py::enum_` before the defs. The default is `py::arg("unit_system") = "metric"`.
9. **`__init__.py`.** Export `UnitSystem`.
10. **README.** Signatures, the enum, both candidate lists, the meaning of "fits", the error behavior, corrected examples, the placeholder note removed, and the imperial-always-fits note.
11. `make format && make check && make install && make test`.

## Risks (none blocking) and orchestrator decisions

1. **Empty input with an invalid epsilon.** It throws `invalid_argument`, because epsilon is validated first. **Accepted:** the spec's error rule is unconditional.
2. **`epsilon = +inf`.** It is accepted and not tested. **Accepted.**
3. **Several errors in one input.** Non-finite values are reported first, because of the upfront pass.
4. **Float32 epsilon.** Python reference checks must cast eps through `np.float32`.
5. **Python rounding.** Use half-away-from-zero, not `np.round`.
6. **C++ random data.** `uniform_real_distribution` differs between standard libraries, so the data is reproducible per toolchain. The tests check invariants, not fixed values.
7. **Existing `ArrayMatchesVector` and benchmarks.** They pass unchanged: the factor is 100 for `{0.4, 1.6, -2.5, 10.49}`, and on `large_values` it is 100 (metric) and 54 (imperial).
8. **Python ints.** `0` passed as `unit_system` is rejected.
