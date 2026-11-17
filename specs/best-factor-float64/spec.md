# Best factor search for any floating type (float32 and float64)

## Goal

Complete the best factor search: we want a version that accepts floating values of any type, for data where the unit system isn't clear.

In concrete terms:
- `best_discretization_factor` works natively on **float64** as well as float32, in C++ (`double` versions) and in Python.
- In Python, the dtype decides which version runs. `float32` input keeps float32 behavior. Everything else (float64, lists, pandas float64, float16, integers) runs in float64, with no lossy conversion to float32.
- When the unit system isn't known, `unit_system=None` (search metric and imperial together) remains the answer. It is now documented as the recommended default for unknown data.

**Why float64 matters:**
- Today Python converts every input to float32, so float64 data loses precision before the search starts.
- Float32 noise also makes the search chase large factors: `[0.1, 2.5, -3.7]` with no budget returns 10⁸, and `[0.3]` searched over both lists returns 2²⁴.
- In float64, `round(v·f)/f` hits the same double exactly for "nice" values, so the error is exactly 0 and the smallest-factor tie rule gives the intuitive answer: 10 for both examples.

**Depends on** [specs/best-factor-search](../best-factor-search/spec.md), which is on branch `feature/best-factor-search` and must be merged into `main` first.

## Definitions

These are unchanged from best-factor-search, with `v` now being the input value converted to `double`. That conversion is exact for float16, float32 and float64, and for integers up to 2⁵³.
- `q = round(v·f)`, rounding halfway away from zero.
- `err(f) = mean |q/f − v|`, computed in `double` with true division.
- Candidates, budget, int32 overflow cutoff, tie rule (smallest factor), and how `unit_system` is attributed: all identical.

## End goal / acceptance criteria

Float64 selection. These go through the C++ `double` versions and Python `float64` input. Errors are checked with `==` when the expected value is 0, otherwise with a relative tolerance of 1e-12. M = Metric, I = Imperial, B = both lists (`unit_system` omitted/`None`), D = default budget.

- [ ] `{0.1, 2.5, -3.7}`, M, 1000 → `10`, error `0` (float32 gives 1.64e-8)
- [ ] `{0.1, 2.5, -3.7}`, M, D → `10`, error `0` (float32 gives 10⁸)
- [ ] `{0.1}`, M, D → `10`; `{0.1}`, B, D → `10`, Metric (float32 gives 134217728, Imperial)
- [ ] `{0.3}`, B, D → `10`, Metric, error `0` (float32 gives 16777216, Imperial)
- [ ] `{1.234}`, M, D → `1000`, error `0`
- [ ] `{12.7, 2.54, 30.48}`, B, D → `100`, Metric, error `0`
- [ ] `{0.375, 1.5625, 2.75}`, B, D → `16`, Imperial, error `0`
- [ ] `{1/3}`, I, 100 → `3`, error `0`; `{1/3}`, B, D → `3`, Imperial
- [ ] `{1/3, 0.25}`, I, 100 → `12`, error `0`; `{7/12}`, B, D → `12`, Imperial
- [ ] `{0.4, 1.6, -2.5}`: I, 10 → `8`, error `0.016666666666666646`; M, 10 → `10`, error `0`; B, 10 → `10`, Metric
- [ ] The same results as float32 where the input is exactly representable in both: `{0.125}` M 100 → `100` (error ≈ 0.005); `{0.125}` M D → `1000`; `{0.5, 0.25}` M D → `100`; `{2.5}` M 1 → `1` (0.5); `{3, -7, 0}` → `1`; `{0.3125}` I 1000 → `16`; `{1e9}` M D → `1`
- [ ] Errors and edge cases in float64:
  - empty input → `{1, 0, Metric}`;
  - NaN or ±inf → `invalid_argument` / `ValueError`;
  - `{3e9}` and `{1e300}` → `out_of_range` / `ValueError`;
  - `max_factor < 1` or `num_threads < 0` → `invalid_argument` / `ValueError`.

Python dtype dispatch:

- [ ] `float32` input (ndarray, strided float32 view, float32 Series) gives **exactly** the float32 result. `[0.1, 2.5, -3.7]` with M and D → `100000000`, identical to the C++ `float` version.
- [ ] `float64` ndarray, a plain list, a float64 Series and a DataFrame column give the float64 result: the same input → `10`, error `0`.
- [ ] `float16` input is widened to float64 (exactly): `np.array([0.1], dtype=np.float16)` (= 0.0999755859375), B, D → `8192`, Imperial, error `0`.
- [ ] Integer input (`int32`, `int64`) is handled as float64: `[3, -7, 0]` → `1`, error `0`.
- [ ] Contiguous float32 and contiguous float64 arrays are passed to C++ **without copying**. Other layouts and dtypes are converted once.

Parallelism, determinism and accuracy, for float64:

- [ ] On 1M random float64 values (fixed seed, uniform in [−1000, 1000]), each system choice: the factor, and the error **bit-identical**, are the same for `num_threads ∈ {0, 1, 2, 3, 8, 64}`.
- [ ] With auto threads, fewer than 100 000 float64 values spawn no threads (existing spawn counter).
- [ ] Property: 10 000 random float64 values (fixed seed), budgets `{10, 1000, 10⁶}` × each system choice. The result equals a brute-force serial reference in double, including exact error equality in C++.

Performance (measured by benchmarks, recorded in the report, not asserted):

- [ ] 50M float64 values (fixed seed, uniform in [−1e6, 1e6]), Metric, D: auto threads **≥ 3×** faster than `num_threads = 1`. Also record float32 vs float64 time at the same size.

Compatibility and docs:

- [ ] All existing C++ tests compile and pass **unchanged**. That includes the `best_discretization_factor(nullptr, 0, …)` calls and every call with a `std::vector<float>`: adding the `double` versions must not create ambiguous overloads.
- [ ] The README documents the double versions, the Python dtype dispatch rule, the float32-vs-float64 difference (with the `[0.3]` example), and `unit_system=None` as the recommended choice when the unit system is unknown. The README example `best_discretization_factor([0.3]).factor  # 16777216` is corrected: a list is now float64, so it gives `10`.
- [ ] `make check` and `make install && make test` pass.

## API

C++ (`cpp/include/discretization/discretization.hpp`). The existing `float` versions stay as they are. Add `double` versions with the same parameters and defaults:

```cpp
FactorResult best_discretization_factor(const std::vector<double> &values,
                                        std::optional<UnitSystem> unit_system = std::nullopt,
                                        int max_factor = std::numeric_limits<int>::max(),
                                        int num_threads = 0);
FactorResult best_discretization_factor(const double *values, std::size_t size, /* same defaults */);
```

- One implementation, templated on the element type and shared by both types. Same chunking (65 536 elements), same threshold, same single-pass, deterministic reduction.
- **No ambiguity:** `best_discretization_factor(nullptr, 0, …)` must still compile and call the `float` version. The `double` raw-buffer version is therefore a constrained template (for example `template <std::same_as<double> T> FactorResult best_discretization_factor(const T *values, …)`), which `nullptr` can't deduce. Another solution with the same guarantee is also fine, for example an explicit `std::nullptr_t` overload. Keep the vector version delegating to the raw-buffer version, as CLAUDE.md requires.

Python: the signature is unchanged, `best_discretization_factor(values, unit_system=None, max_factor=2**31 - 1, num_threads=0) -> FactorResult`.

- Dispatch: if the input's dtype (`np.asarray(values).dtype`, without forcing a dtype) is `float32`, call the float version. Otherwise call the double version with a float64 array. Non-numeric input keeps raising an error, as it does today (`TypeError` or `ValueError`).
- Zero copy for contiguous float32 and float64. The GIL is released, the 1-D check stays, and `out_of_range` becomes `ValueError`, all as before.

## Required tests

- [x] **C++ unit tests** (`tests_cpp/test_best_factor_float64.cpp`): every float64 selection, edge and error criterion through the `std::vector<double>` and `const double *` versions. Add a compile check that `nullptr` calls and `std::vector<float>` calls still pick the float version (for example with `static_assert` on `decltype` or overload resolution, or by simply keeping the existing tests compiling).
- [x] **Python unit tests** (`tests_py/test_best_discretization_factor_float64.py`, MAGE style): the float64 examples; one test per dispatch case (float32, float64, list, float16, int32/int64, float32 Series, float64 Series, DataFrame column, strided float32 and float64 views); the errors.
- [x] **Edge cases**: empty input; a single value; `{1e300}` and `{3e9}` overflow; NaN/±inf; subnormal doubles (`{5e-324}` → `1`, error ≈ 5e-324); negative values; integers above 2²⁴ that float32 can't represent exactly (`[16777217]` as int64 → `1`, error `0`; as float32 it would be 16777216).
- [x] **Property / accuracy tests**: the float64 brute-force reference (C++ exact equality, Python `rel_tol` 1e-12 with an argmin check). Determinism over the thread counts for float64, in C++ and Python.
- [x] **Python input types**: covered by the dispatch tests above.
- [x] **Benchmarks**: `test_huge_values_float64[threads=1|auto]` (50M float64, `pedantic`, `rounds=3`) and `test_large_values_float64[None|metric|imperial]` (1M). No speedup assertion; record in the report.
- [x] **Regression tests**: the existing tests stay unchanged, with **one authorized exception**. In `TestBestDiscretizationFactorInputTypes::test_input_types` (`tests_py/test_best_discretization_factor.py`), the `list`, `float64`, `series` and `dataframe` kinds are no longer float32 data. Compare them with the **float64** reference result (`np.array(Constants.VALUES, dtype=np.float64)`) instead of the float32 one. The `float32` and `strided` kinds keep comparing with the float32 reference. The expected factors and unit systems in that test (`10` / `10` / `8`) don't change. No other existing test may be edited.

## Constraints

- No new dependencies. C++20 only. Keep `std::thread` and the existing chunking and determinism design.
- One templated core for float and double: no copy-pasted implementation.
- Don't convert float64 input to float32 anywhere.
- Float32 behavior (C++ and Python float32 input) must not change by a single bit. The existing float32 tests and benchmarks are the guard.
- Keep the C++ core free of Python.

## Out of scope

- float64 support for `calculate_discretization_factor` and `discretize` (see Open questions).
- `long double`, a native float16 path (float16 is widened to float64), complex numbers.
- Unit-system detection, other candidate lists, any-integer factors, a tie tolerance, other error metrics.

## Delivery

- [x] Commit on a feature branch `feature/best-factor-float64`
- [ ] Push the branch and open a pull request

## Open questions

Resolved with the user while drafting:
- Types: float32 and float64 natively; other dtypes are converted to float64.
- Unknown unit system: searching both lists (`unit_system=None`) is enough. Document it as the recommended choice.
- Delivery: a new follow-up spec, not an amendment to best-factor-search.

Remaining:
- **Should `calculate_discretization_factor` and `discretize` also get float64 versions?** **Answered at the start of `/implement-spec`: no, only `best_discretization_factor` in this run.** This spec leaves them out to keep the change focused. Adding them would change Python results for float64 and list inputs in many existing tests, for example the imperial factor for `[0.4, 1.6, -2.5]`. That would need its own regression list. Proposed as a follow-up spec.
- **Order:** `/implement-spec` branches from `main`, so merge `feature/best-factor-search` into `main` first.
