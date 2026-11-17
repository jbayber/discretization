# Unit-system discretization factors (metric and imperial)

## Goal

Add various discretization factors. Think about imperial units and metric units. For metric it's rather easy, as we'll need 10, 100, 1000, etc. Imperial is more tricky.

In concrete terms: replace the placeholder `calculate_discretization_factor` (always `1`) with a real one. It picks the **smallest** factor from the chosen unit system's candidate list such that every value, scaled by the factor and rounded, is within `epsilon` of the original. `discretize` then returns the values scaled by that factor and rounded to `int32`.

- **Metric** candidates: `1, 10, 100, …, 10^9`.
- **Imperial** candidates: every number of the form `2^a · 3^b` that fits in `int32` (≤ 2 147 483 647), in ascending order: `1, 2, 3, 4, 6, 8, 9, 12, 16, 18, 24, 27, 32, 36, 48, 54, 64, 72, 81, 96, …` (328 numbers). This covers binary inch fractions (1/2 … 1/64 in), 12 in/ft, 3 ft/yd, 36 in/yd and 16 oz/lb.

## Definitions

For a float32 input `v` (converted to `double`) and a candidate factor `f`:

- `q = round(v · f)` in `double`, with halfway cases rounded away from zero (`std::lround` / `std::round` semantics).
- error = `|q / f − v|`, computed in `double`.
- `f` **fits** the input if the error is `≤ epsilon` for every value.

`calculate_discretization_factor` returns the first candidate, in ascending order, that fits. `discretize` returns `q` for every value, using that factor.

## End goal / acceptance criteria

Metric (default):

- [ ] `calculate_discretization_factor({0.1, 2.5, -3.7}, 0.01)` → `10`
- [ ] `calculate_discretization_factor({1.234}, 0.001)` → `1000`, and with `epsilon = 0.005` → `100` (smallest factor that fits, not the exact one)
- [ ] `calculate_discretization_factor({3, -7, 0}, 0.01)` → `1`
- [ ] `calculate_discretization_factor({1/3}, 1e-6)` → `1000000`
- [ ] Empty input → factor `1`, and `discretize` returns an empty result (both systems)
- [ ] `discretize({0.4, 1.6, -2.5}, 0.01)` → `{4, 16, -25}`
- [ ] `discretize({0.0, 1.2, 1.7, -1.2, -1.7}, 0.5)` → `{0, 1, 2, -1, -2}` (factor 1)
- [ ] `discretize({0.5, 1.5, 2.5, -0.5, -2.5}, 0.5)` → `{1, 2, 3, -1, -3}` (factor 1, halfway away from zero)
- [ ] Omitting the unit system gives the same result as passing `Metric` explicitly

Imperial:

- [ ] `{0.3125}` (5/16), `1e-6` → `16`; `discretize` → `{5}`
- [ ] `{1/3}`, `1e-6` → `3`; `discretize` → `{1}`
- [ ] `{7/12}`, `1e-6` → `12`; `discretize` → `{7}`
- [ ] `{2.25, 0.5}`, `1e-6` → `4`; `discretize` → `{9, 2}`
- [ ] `{1/3, 0.25}`, `1e-6` → `12`; `discretize` → `{4, 3}`
- [ ] `{0.5, 0.25, 0.3125}`, `1e-6` → `16`; `discretize` → `{8, 4, 5}`
- [ ] `{0.1, 2.5, -3.7}`, `0.01` → `48`; `discretize` → `{5, 120, -178}`
- [ ] `{0.4, 1.6, -2.5}`, `0.01` → `32`; `discretize` → `{13, 51, -80}`

Errors (both functions, both systems unless stated):

- [ ] `epsilon ≤ 0` or NaN → C++ `std::invalid_argument`, Python `ValueError`
- [ ] Any NaN or ±inf value → C++ `std::invalid_argument`, Python `ValueError`
- [ ] No candidate fits: `{0.1}`, `1e-12`, metric → C++ `std::out_of_range`, Python `ValueError`
- [ ] `round(v · f)` overflows `int32` before a candidate fits: `{3e9}`, `0.5` → C++ `std::out_of_range`, Python `ValueError`. The search stops at the first overflow, because larger factors only make it worse.
- [ ] Python: an unknown `unit_system` (for example `"cubits"`) → `ValueError`

General:

- [ ] Property: for 10 000 random values (fixed seed, uniform in [−1000, 1000]) and `epsilon ∈ {0.1, 0.01, 0.001}`, for both systems: the returned factor fits, `|discretize(v)[i] / f − v[i]| ≤ epsilon` for every `i`, and the previous candidate in the list does not fit (or `f` is the first candidate)
- [ ] Vector and raw-buffer C++ versions give identical results for every case above
- [ ] README API section is updated: signatures, the `UnitSystem` enum, both candidate lists, the error behavior, and the placeholder note removed
- [ ] `make check` and `make install && make test` pass

## API

C++ (`cpp/include/discretization/discretization.hpp`):

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

The vector versions delegate to the raw-buffer versions, as in the rest of the project. The `UnitSystem` parameter goes **after** `out` in the raw `discretize`, so existing calls still compile.

Python (`discretization` package):

```python
calculate_discretization_factor(values, epsilon, unit_system="metric") -> int
discretize(values, epsilon, unit_system="metric") -> np.ndarray[int32]
```

- `unit_system` is keyword-or-positional. It accepts the strings `"metric"` / `"imperial"` (case-insensitive) or the bound enum `discretization.UnitSystem.METRIC` / `.IMPERIAL`. Anything else raises `ValueError`.
- Export `UnitSystem` from `python/src/discretization/__init__.py` and add it to `__all__`.
- Both C++ exceptions must surface as `ValueError`. pybind11 maps `std::invalid_argument` to `ValueError` but maps `std::out_of_range` to `IndexError`, so the binding has to translate `std::out_of_range` itself.
- Keep the existing binding conventions: zero copy for contiguous float32, `ValueError` for input that isn't 1-D, and the GIL released around the C++ call.

## Required tests

- [x] **C++ unit tests** (GoogleTest, `tests_cpp/`): every acceptance-criterion example for both functions and both systems, through both the vector and raw-buffer versions. A test that the default argument equals `UnitSystem::Metric`.
- [x] **Python unit tests** (pytest, `tests_py/`): the same examples, parametrized over the unit system, passing it as both the string and the enum. Default = metric. Rejection of an unknown `unit_system`. MAGE style: keep `test_calculate_discretization_factor.py` and `test_discretize.py`, with shared data in `Constants`.
- [x] **Edge cases**: empty input, NaN and ±inf values, `epsilon` of `0`, negative or NaN, no candidate fits, `int32` overflow (`{3e9}`), negative values, values that are already integers (factor 1), and a single-element input.
- [x] **Property / accuracy tests**: the randomized criterion above (seed fixed, 10 000 values, 3 epsilons, both systems), in both C++ and Python. Check that the factor is the smallest that fits by also checking the previous candidate.
- [x] **Python input types**: the existing list / float32 / float64 / strided / Series / DataFrame-column tests keep passing, with their expected outputs updated to the new behavior (see Regression). Add an imperial case to the parametrization.
- [x] **Benchmarks**: `calculate_discretization_factor` and `discretize` on the existing 1M-value `large_values` fixture with `epsilon = 0.01`, for both systems. No hard target. Record the numbers in the report.
- [x] **Regression tests**: The existing placeholder tests are **explicitly authorized** to change, because they pin placeholder behavior that this spec replaces (the README calls it a placeholder). Change only what's listed here. Every other existing test must keep passing unchanged.
  - C++ `CalculateDiscretizationFactor.VectorReturnsOne`: rename it (for example `VectorMetric`) and expect `10`.
  - C++ `Discretize.VectorRoundsToNearest` and `Discretize.HalfwayRoundsAwayFromZero`: change `epsilon` to `0.5`. The expected outputs stay the same, because the factor is 1.
  - Python `TestCalculateDiscretizationFactor.test_returns_one`, `test_series` and `test_dataframe_column`: expect `10` (rename `test_returns_one`).
  - Python `TestDiscretize.test_rounding`: use `epsilon = 0.5`. The expected outputs stay the same.
  - `Constants.DISCRETIZED`: `[0, 2, -3]` becomes `[4, 16, -25]` (metric, `EPSILON = 0.01`).
  - `EmptyVectorReturnsOne`, `ArrayMatchesVector`, `test_empty`, `test_rejects_multidimensional_input` and the benchmarks stay as they are.

## Constraints

- No new dependencies.
- The arithmetic is done in `double` from the float32 inputs. The result type stays `int` / `int32`.
- The candidate lists are compile-time or static data, not recomputed on every call.
- Factor search: for each candidate, stop scanning values at the first one that doesn't fit. Single-threaded is fine.
- `calculate_discretization_factor` and `discretize` must agree: `discretize` uses exactly the factor that `calculate_discretization_factor` returns for the same arguments.
- Keep the C++ core free of Python. Only `python/src/bindings.cpp` includes pybind11.
- Note: every finite float32 is a dyadic rational, so for any input that doesn't overflow, the imperial system eventually finds a fitting power of two (for example, `0.1f` fits at `2^27`). The "no candidate fits" error is therefore mainly reachable in metric, and imperial fails mainly through overflow. This is expected.

## Out of scope

- Unit conversion (for example inches ↔ cm). Values are assumed to already be in the user's base unit.
- Caller-provided candidate lists, or other unit systems.
- Returning the factor from `discretize` (call `calculate_discretization_factor` for it).
- An inverse (`undiscretize`), 64-bit outputs, multithreading/SIMD.

## Delivery

- [x] Commit on a feature branch `feature/unit-system-factors`
- [ ] Push the branch and open a pull request

## Open questions

Resolved with the user while drafting:
- Imperial candidates: all `2^a · 3^b` within int32.
- API: a `UnitSystem` enum as a trailing parameter, defaulting to `Metric`. In Python, `unit_system` takes a string or the enum.
- Placeholder tests: replace the placeholder behavior and update the listed tests.
- Failure: throw (`invalid_argument` / `out_of_range`), and raise `ValueError` in Python.

Remaining (non-blocking):
- Should `epsilon` be inclusive (`≤`)? This spec says yes.
