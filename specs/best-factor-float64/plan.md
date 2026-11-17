# Plan: Best factor search for any floating type (float32 and float64)

Written by the `planner` subagent, condensed by the orchestrator. Orchestrator decisions on the planner's risks are at the end and are binding.

The planner re-verified every expected value in the spec with an independent double reference, including float16 → 8192, the subnormal case and 16777217. Scratch files: `/private/tmp/claude-501/-Users-washingmachine-dev-discretization/d752f546-dc99-4863-8212-b18a7f2420f4/scratchpad/{ref.py,prop.cpp,sa.cpp}`.

## API

C++ (`discretization.hpp`, add `<concepts>`). The float declarations stay as they are. Add:

```cpp
FactorResult best_discretization_factor(const std::vector<double> &values,
                                        std::optional<UnitSystem> unit_system = std::nullopt,
                                        int max_factor = std::numeric_limits<int>::max(), int num_threads = 0);

// Constrained template so that `nullptr` (not deducible) and `float *` still resolve to the float overload.
template <std::same_as<double> T>
FactorResult best_discretization_factor(const T *values, std::size_t size,
                                        std::optional<UnitSystem> unit_system = std::nullopt,
                                        int max_factor = std::numeric_limits<int>::max(), int num_threads = 0);

extern template FactorResult best_discretization_factor<double>(const double *, std::size_t,
                                                                std::optional<UnitSystem>, int, int);
```

The planner checked this with Apple clang 21: all five existing `tests_cpp/*.cpp` compile unchanged, and the routing is correct:
- `nullptr`, `vector<float>`, `float*` and `const float*` go to the float version;
- `vector<double>`, `double*` and `const double*` go to the double version.

**Python:** the signature is unchanged. `values` becomes a `py::object` with dtype dispatch:
- Native float32 ndarrays, Series and strided views take the float path.
- Everything else (float64, lists, float16, ints, bool) takes the double path.
- There is no copy for contiguous float32 or float64.
- The error types stay as they are today: `None` and scalars raise `ValueError`, `["a"]` raises `TypeError`.

## Files

| File | Change |
|---|---|
| `cpp/include/discretization/discretization.hpp` | double declarations, `extern template`, comment updated for double |
| `cpp/src/discretization.cpp` | float body → `template <typename T> BestFactor(...)` in the anonymous namespace; public float, double template, explicit instantiation and `vector<double>` versions delegate to it |
| `python/src/bindings.cpp` | `DoubleArray`, dtype dispatch, templated `RunBestFactor` |
| `python/src/discretization/__init__.py` | docstring |
| `README.md` | double API, dispatch rule, zero copy, float32-vs-float64 explanation with `[0.3]`, `unit_system=None` recommended, fixed example `[0.3]` → 10 (plus a float32 example → 16777216), "pass a typed `std::vector`" note, test-layout entry |
| `tests_cpp/test_best_factor_float64.cpp` (new) | C++ tests |
| `tests_py/test_best_discretization_factor_float64.py` (new) | Python tests |
| `tests_py/constants.py`, `conftest.py` | additions only |
| `tests_py/test_best_discretization_factor.py` | only the authorized `test_input_types` edit |

## Expected values (planner reference, double)

`{0.1,2.5,-3.7}`:
- M 1000 → 10, error 0
- M D → 10, error 0

`{0.1}` and `{0.3}`:
- `{0.1}` M D → 10
- `{0.1}` B D → 10, Metric
- `{0.3}` B D → 10, Metric, error 0

Decimal and fraction data:
- `{1.234}` M D → 1000, error 0
- `{12.7,2.54,30.48}` B D → 100, Metric, error 0
- `{0.375,1.5625,2.75}` B D → 16, Imperial, error 0
- `{1/3}` I 100 → 3, error 0
- `{1/3}` B D → 3, Imperial
- `{1/3,0.25}` I 100 → 12, error 0
- `{7/12}` B D → 12, Imperial

`{0.4,1.6,-2.5}`:
- I 10 → 8, error 0.016666666666666646
- M 10 → 10, error 0
- B 10 → 10, Metric
- B 1 → 1, error 0.4333333333333333

`{0.125}`:
- M 100 → 100, error 0.0050000000000000044 (compared with 0.005 at relative tolerance 1e-12)
- M D → 1000
- M 50 → 10, error 0.024999999999999994

Small and integer cases:
- `{0.5,0.25}` M D → 100
- `{2.5}` M 1 → 1, error 0.5
- `{3,-7,0}` M/I/B → 1, error 0, Metric
- `{0.3125}` I 1000 → 16
- `{1e9}` M D → 1
- `{-0.125}` M D → 1000
- `{-0.3125}` I D → 16

`{1e9,0.5}`:
- M → 1, error 0.25
- I → 2, error 0
- B → 2, Imperial

Edge cases:
- `{5e-324}` M/I/B → 1, error exactly `denorm_min`, Metric
- `{16777217}` M/I/B → 1, error 0
- `{16777217.5}` B D → 2, Imperial, error 0
- `{0.0999755859375}`: B D → 8192, Imperial, error 0; I D → 8192; M D → 1e9, error 6.250000517127319e-11
- `{2147483647, -2147483647}` B → 1, error 0
- `{2147483647.4}` B → 1, error 0.40000009536743164

**`out_of_range`:** `{2147483647.5}`, `{-2147483648}`, `{3e9}`, `{0.5,-3e9}`, `{2147483648.0}`, `{1e300}`, `{1e39}`, `DBL_MAX`.

**Float32 comparisons (float version):**
- `{0.1f,2.5f,-3.7f}` M D → 100000000, error 1.2688000557610952e-09
- `{0.1f,2.5f,-3.7f}` M 1000 → 10, error 1.6391277252170156e-08
- `{0.1f}` B D → 134217728, Imperial
- `{0.3f}` B D → 16777216, Imperial

## C++ tests (`tests_cpp/test_best_factor_float64.cpp`)

Every suite name is prefixed `BestFactorFloat64…`. The helpers (`ExpectError`, `ExpectResult`, `ExpectIdentical`, `Raw`, candidate lists, a serial double `Reference`) are copied into an anonymous namespace; `test_best_factor.cpp` is not edited.

**Portable generator.** No `uniform_real_distribution`, and no bare `a + b*c`:

```cpp
std::vector<double> RandomDoubles(std::uint64_t seed, std::size_t size, double low, double high) {
  std::mt19937_64 gen{seed};
  std::vector<double> values(size);
  for (auto &v : values)
    v = std::fma(high - low, static_cast<double>(gen() >> 11) * 0x1p-53, low);
  return values;
}
```

### Selection (TEST_P over a table)

Each row is checked through `Vector`, `Raw`, `RawMatchesVector`, `MatchesReference` (bit-identical) and `SingleThreadMatchesAuto`. The rows are every float64 selection value above, the "same as float32" rows, and the edge rows:
- NegativeEighth, NegativeFiveSixteenths, Subnormal;
- AboveTwoTo24, HalfAboveTwoTo24, Float16Tenth;
- Int32Limit, NearInt32Limit;
- OverflowHalf for M, I and B, MaxFactorOneBoth.

### Edge

- **Empty input**, every system → `{1, 0, Metric}`:
  - an empty vector;
  - `Raw(empty)`;
  - `static_cast<const double*>(nullptr), 0`, with and without `(1, 8)`.
- **Single value** `{0.1}` → 10.
- **AllZeros** (1000 values), every system → 1, Metric.
- **FactorOneReportsMetricUnderImperial.**
- **NegativeSubnormal.**

### Errors

- **NonFiniteValues** (including `{3e9, NaN}`), every system, vector and raw → `invalid_argument`.
- **OverflowAtFactorOne:** the `out_of_range` list above × every system × the default budget and budget 1.
- **InvalidMaxFactor:** {0, -1, INT_MIN}, also on empty input.
- **NegativeNumThreads:** {-1, -8, INT_MIN}.
- **InvalidUnitSystemEnum.**
- **ErrorsFromLargeParallelInput:** 1M values from `RandomDoubles(7, …)`, threads {0, 3, 8}:
  - NaN at the end → `invalid_argument`;
  - -inf in the middle → `invalid_argument`;
  - 3e9 at the end → `out_of_range`.

### Overloads (namespace-scope `static_assert`s)

```cpp
template <class P> concept RawCallable = requires(P p) { best_discretization_factor(p, std::size_t{0}); };
template <class P> concept TemplateCallable = requires(P p) { best_discretization_factor<>(p, std::size_t{0}); };
template <class V> concept VectorCallable = requires(const V &v) { best_discretization_factor(v); };
static_assert(RawCallable<std::nullptr_t>);
static_assert(!TemplateCallable<std::nullptr_t>);
static_assert(TemplateCallable<const double *> && TemplateCallable<double *> && !TemplateCallable<const float *>);
static_assert(RawCallable<const float *> && RawCallable<float *>);
static_assert(!RawCallable<const int *> && !RawCallable<const long double *>);
static_assert(VectorCallable<std::vector<float>> && VectorCallable<std::vector<double>> && !VectorCallable<std::vector<int>>);
```

Runtime tests:
- `AddressOfEachOverload`: take a function pointer to each overload and check it gives the matching result.
- `DefaultArguments`: `vector<double>{0.3}` → 10, Metric, identical to the call with explicit defaults.

### Float32 vs float64

- **DifferentWhereFloat32IsInexact:** the float32 comparisons above.
- **IdenticalWhereExactlyRepresentable:** the double and float versions give `ExpectIdentical` results on these inputs:
  - `{0.125}` M 100 and M D;
  - `{0.5,0.25}` M D;
  - `{2.5}` M 1;
  - `{3,-7,0}`, every system;
  - `{0.3125}` I 1000;
  - `{1e9}` M D.
- **Float64OfFloat32DataMatchesFloatVersion:** 10k float32 values from `mt19937{42}`, `std::fma(2000.0f, float(gen()>>8)*0x1p-24f, -1000.0f)`, widened to double. The double result must be bit-identical to the float result, for 3 system choices × budgets {10, 1000, 1e6}.

### Threads

- `NoSpawnBelowThreshold`: `kAutoParallelThreshold - 1` values, with the vector version under M and the raw version under B. Spawn-counter delta is 0.
- `NoSpawnWithOneThread`: 1M values, `num_threads = 1`. Delta is 0.
- `SpawnsAboveThreshold`: 1M values, `num_threads = 3`. Delta is > 0 and ≤ 4.

### Determinism (TEST_P over M, I and B)

`RandomDoubles(12345, 1'000'000, -1000, 1000)`. Threads {0, 2, 3, 8, 64} and the raw version with 3 threads must be bit-identical to the 1-thread result.

### Property (TEST_P)

`RandomDoubles(42, 10'000, -1000, 1000)`, which fits in one chunk. The vector version, the raw version and `num_threads = 4` must all be `ExpectIdentical` to the `Reference`.

Hardcoded factors are also asserted:

| system | budget 10 | budget 1000 | budget 1e6 |
|---|---|---|---|
| Metric | 10 | 1000 | 1000000 |
| Imperial | 9 | 972 | 995328 (gap 4.3%) |
| Both | 10 Metric | 1000 Metric | **reference only** (gap 0.25%, see Decisions) |

## Python tests (`tests_py/test_best_discretization_factor_float64.py`, MAGE style)

The helpers `ENUMS`, `_assert_error` and `_assert_identical` are copied into the new file; the existing file is not edited.

### Additions to `constants.py`

- `BEST_FACTOR_FLOAT64_EXAMPLES`: the selection table above, in the same layout as `BEST_FACTOR_EXAMPLES`.
- `BEST_FACTOR_FLOAT64_OVERFLOW_VALUES`.
- `BEST_FACTOR_FLOAT64_PROPERTY_EXPECTED`.
- `BEST_FACTOR_LARGE_FLOAT64_EXPECTED = {None: (2048, "imperial"), "metric": (1000, "metric"), "imperial": (2048, "imperial")}` (gap 5.5%).
- `ALLOCATION_SIZE = 1_000_000`.

### Additions to `conftest.py`

- `property_values_float64` (seed 42, 10k values) and `determinism_values_float64` (seed 7, 1M values), built exactly: `rng.integers(-(2**52), 2**52, n, dtype=np.int64) * (1000.0 / 2**52)`.
- `large_values_float64` (`default_rng(0).uniform(-1e6, 1e6, BENCHMARK_SIZE)`, with no `astype`).
- `huge_values_float64` (`HUGE_SEED`, `HUGE_SIZE`, with no `astype`).
- `mean_abs_error_float64`.
- `peak_allocation`: a tracemalloc peak-delta helper. The planner checked that NumPy buffers are traced.

### Test classes

1. **`TestBestDiscretizationFactorFloat64`**
   - `test_examples`: float64 arrays, every spelling of the unit system.
   - `test_examples_list`.
   - `test_examples_keywords` (`num_threads = 1`).
   - `test_defaults`: `[0.3]` → 10, Metric, identical to the call with explicit defaults.
   - `test_list_0_3_is_float64`.
2. **`TestBestDiscretizationFactorDtypeDispatch`**
   - float32 ndarray `[0.1,2.5,-3.7]` M D → 100000000.
   - float32 strided view and float32 Series give the identical float32 result.
   - float64 ndarray → 10, error 0.
   - A list, a float64 Series and a DataFrame column are identical to float64.
   - A float64 strided view is identical to contiguous float64.
   - float16 `[0.1]` B D → 8192, Imperial; also assert the widened value is `0.0999755859375`.
   - int32/int64 `[3,-7,0]` → 1, every system.
   - int64 `[16777217]` → 1, error 0.
   - float64 `[16777217.5]` B → 2, Imperial (the float32 path gives 1).
   - `[0.3]`: float32 → 16777216 Imperial, float64 → 10 Metric.
3. **`TestBestDiscretizationFactorCopies`** (`peak_allocation`, n = 1M; metric with budget 10 so each call is fast):

   | Input | Peak allocation |
   |---|---|
   | contiguous float32 | < 1 MB |
   | contiguous float64 | < 1 MB |
   | strided float64 | in [8n, 16n) |
   | strided float32 | in [4n, 8n) |
   | float16 | in [8n, 16n), which proves it is widened to float64 |
   | int32 | in [8n, 16n) |

4. **`TestBestDiscretizationFactorFloat64Errors`**
   - Non-finite values, plus `[3e9, nan]` → `ValueError`.
   - The overflow list with the default budget and budget 1 → `ValueError`.
   - Invalid `max_factor` / `num_threads` → `ValueError`.
   - 2-D float64 and int64 input → `ValueError`.
   - The scalar `0.5` and `None` → `ValueError`.
   - `["a","b"]` → `(TypeError, ValueError)`.
   - An unknown unit system → `ValueError`.
   - Empty float64 array and `[]` → `{1, 0, METRIC}`.
5. **`TestBestDiscretizationFactorFloat64Determinism`**
   - Thread counts {0, 1, 2, 3, 8, 64}, every system: all results identical.
   - Below the auto threshold, auto and 1 thread give identical results.
6. **`TestBestDiscretizationFactorFloat64Property`** (system × budgets {10, 1000, 1e6})
   - Reference over the candidates `f ≤ budget` with `round(max_abs·f) ≤ INT32_MAX`.
   - Error `isclose` at relative tolerance 1e-12.
   - The result is the argmin, with ties going to the smallest factor.
   - Hardcoded factors:

     | system | budget 10 | budget 1000 | budget 1e6 |
     |---|---|---|---|
     | metric | 10 | 1000 | 1000000 |
     | imperial | 9 | 972 | 995328 |
     | None | 10 metric | 1000 metric | **reference only** (gap 0.39%, see Decisions) |

7. **`TestBestDiscretizationFactorFloat64Benchmark`**
   - `test_large_values_float64[None|metric|imperial]`.
   - `test_huge_values_float64[threads=1|auto]`: `pedantic`, `rounds=3` → 1000, Metric.
   - No speedup assertion.

### Regression (the only authorized edit, in `TestBestDiscretizationFactorInputTypes::test_input_types`)

```python
dtype = np.float32 if kind in ("float32", "strided") else np.float64
expected = best_discretization_factor(np.array(Constants.VALUES, dtype=dtype), unit_system, 10)
```

The parametrized expectations 10/10/8 stay; the planner verified them in float64.

## Implementation steps

1. **Header** as shown under API. The definition in the .cpp must repeat exactly `template <std::same_as<double> T>`. Don't repeat default arguments in the definition or in the explicit instantiation.
2. **Core.** Rename the float body to `template <typename T> BestFactor(const T*, ...)` with `static_assert(std::is_floating_point_v<T>)`. Change only the element-typed parts:
   - `chunk_max` becomes `std::vector<T>`;
   - `T m`, `const T a = std::abs(values[i])`;
   - `bad |= !(a <= std::numeric_limits<T>::max())`.

   Everything else stays the same: validation order, cutoff, accumulation array, true division, chunk-order reduction and strict `<`. Then add the four public entry points that delegate to it:
   - the float raw version;
   - the double template;
   - `template FactorResult best_discretization_factor<double>(...)`;
   - the `vector<double>` version.

   **Numerics:**
   - `q` stays a double, so `inf > INT32_MAX` rejects the candidate and the function throws `out_of_range`.
   - Subnormals tie at `denorm_min`, so factor 1 wins.
   - No FTZ and no `-ffast-math`.
3. **Bindings.**
   - `py::array::ensure(values)`. If that fails → `py::type_error`.
   - If the result passes `py::isinstance<py::array_t<float>>`, use `FloatArray::ensure`; otherwise use `DoubleArray::ensure`. If either fails → `py::type_error`.
   - Pass the array to a templated `RunBestFactor`, which holds today's body unchanged: ndim check, unit-system parsing, `max_factor` check and clamp, GIL release, `out_of_range` → `ValueError`.
   - Leave the other two functions untouched, and update the docstring.
4. `__init__.py` docstring and README.
5. `make format`, `make check`, `make install && make test`. Record the benchmarks: float64 auto vs 1 thread, and float32 vs float64 at 50M.

## Decisions on the planner's risks (orchestrator, binding)

1. **Braced-list calls** such as `best_discretization_factor({0.1f})` become ambiguous between `vector<float>` and `vector<double>`. No existing test or README example does this. **Accepted.** The README says to pass a typed `std::vector`.
2. **GCC wasn't verified locally,** so CI is the check. If GCC rejects the constrained template, use the fallback with the same guarantee: a non-template `const double *` overload plus a `std::nullptr_t` overload that forwards to the float version. In that case, replace the `!TemplateCallable<std::nullptr_t>` assert with a check that the nullptr overload exists.
3. **Integer lists convert twice** (list → int64 → float64). **Accepted**; not tested.
4. **Non-native byte order** (`'>f4'`) takes the float64 path. The result is still bit-identical, because both paths do the same double arithmetic. **Accepted**; not tested.
5. **Complex and longdouble** are out of scope and behave as today, except that they are narrowed to float64.
6. **Hardcoded data-dependent factors with a gap under 1%** (C++ Both/1e6 at 0.25%, Python None/1e6 at 0.39%) are **checked against the reference only**, with no hardcoded factor. This follows the earlier Linux CI failure. All other hardcoded factors have gaps of at least 2.3% and exact, portable data.
7. **Fixture memory** is about 600 MB for the 50M float32 and float64 benchmark data. Acceptable.
8. **The ≥ 3× speedup** is recorded, not asserted. Float64 is more bandwidth-bound; if it misses, report it rather than change the design.
9. **The subnormal error** is compared exactly with `denorm_min`.
10. **Helpers are duplicated** in the new test files, because the existing files must not be edited.
