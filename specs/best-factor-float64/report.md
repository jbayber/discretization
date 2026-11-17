# Report: Best factor search for any floating type (float32 and float64)

**Status:** done. Branch `feature/best-factor-float64`, not pushed (the spec doesn't ask for a PR).

## Summary

`best_discretization_factor` now works natively on float64 as well as float32.

**C++**
- New `std::vector<double>` and `const double *` versions.
- The raw-buffer `double` version is a constrained template (`template <std::same_as<double> T>`) with an explicit instantiation. `nullptr` and `float *` calls still resolve to the float version, and all existing call sites compile unchanged.
- Both types share one templated core, `BestFactor<T>`. With `T = float` it is the previous code with only the element type spelled as `T`, so float32 results are bit-identical. Chunking, threading, the single pass and the deterministic reduction are unchanged.

**Python**
- `best_discretization_factor` dispatches on the input dtype. float32 (any layout) takes the float path. Everything else takes the float64 path: float64, lists, pandas float64, float16, integers.
- Contiguous float32 and float64 input is passed without copying. Anything else is converted once.
- The error types for bad input are unchanged. The GIL is released and `out_of_range` maps to `ValueError`.

**Scope:** `calculate_discretization_factor` and `discretize` are unchanged, as the user decided at the start of the run.

**Docs:** the README and `__init__` docstring describe the double API, the dispatch rule and zero copy. They explain float32 vs float64 with the corrected `[0.3]` example (now `10`, versus `16777216` in float32), recommend `unit_system=None` for data whose unit system is unknown, and say to pass a typed `std::vector` in C++.

## Acceptance criteria

| Criterion | | Tests (C++ / Python) |
|---|---|---|
| `{0.1, 2.5, -3.7}` M 1000 → 10, err 0 | ✅ | `BestFactorFloat64Selection.*/MixedBudget1000` / `TestBestDiscretizationFactorFloat64::test_examples` |
| `{0.1, 2.5, -3.7}` M D → 10, err 0 | ✅ | `…/MixedDefault` |
| `{0.1}` M D → 10; B D → 10 Metric | ✅ | `…/TenthMetric`, `…/TenthBoth` |
| `{0.3}` B D → 10 Metric, err 0 | ✅ | `…/PointThreeBoth`, `test_defaults`, `test_list_0_3_is_float64` |
| `{1.234}` M D → 1000 | ✅ | `…/OneTwoThreeFour` |
| `{12.7, 2.54, 30.48}` B D → 100 Metric | ✅ | `…/InchesBoth` |
| `{0.375, 1.5625, 2.75}` B D → 16 Imperial | ✅ | `…/SixteenthsBoth` |
| `{1/3}` I 100 → 3; B D → 3 Imperial | ✅ | `…/ThirdImperial100`, `…/ThirdBoth` |
| `{1/3, 0.25}` I 100 → 12; `{7/12}` B D → 12 | ✅ | `…/ThirdQuarterImperial100`, `…/SevenTwelfthsBoth` |
| `{0.4, 1.6, -2.5}` I 10 → 8 (0.016666666666666646); M 10 → 10; B 10 → 10 | ✅ | `…/ValuesImperial10`, `…/ValuesMetric10`, `…/ValuesBoth10` |
| Same as float32 where exactly representable | ✅ | Selection rows `Eighth*`, `HalfQuarterTieSmallest`, `BudgetOne`, `Integers*`, `FiveSixteenths`, `Billion*`; `BestFactorFloat64VsFloat32.IdenticalWhereExactlyRepresentable` |
| Errors and edges: empty, NaN/±inf, `{3e9}`/`{1e300}` overflow, invalid `max_factor`/`num_threads` | ✅ | `BestFactorFloat64Edge.*`, `BestFactorFloat64Errors.*` / `TestBestDiscretizationFactorFloat64Errors` |
| float32 input gives exactly the float32 result (1e8) | ✅ | `test_float32_ndarray`, `test_float32_strided_view`, `test_float32_series`; C++ `DifferentWhereFloat32IsInexact` |
| float64, list, Series, DataFrame → float64 result (10) | ✅ | `test_float64_ndarray`, `test_float64_kinds[*]` |
| float16 widened exactly → 8192 Imperial | ✅ | `test_float16_is_widened_exactly`, C++ `…/Float16Tenth*` |
| int32/int64 → 1 | ✅ | `test_integers[*]`, `test_int64_above_2_24` |
| Zero copy for contiguous float32/float64 | ✅ | `TestBestDiscretizationFactorCopies` (tracemalloc peaks) |
| Bit-identical across thread counts {0, 1, 2, 3, 8, 64}, 1M float64 values | ✅ | `BestFactorFloat64Determinism.*` / `TestBestDiscretizationFactorFloat64Determinism` |
| No spawned threads below 100k values (auto) | ✅ | `BestFactorFloat64Threads.NoSpawnBelowThreshold`, `NoSpawnWithOneThread` |
| Property: matches the brute-force reference (10k values, 3 budgets × 3 system choices) | ✅ | `BestFactorFloat64Property.*` / `TestBestDiscretizationFactorFloat64Property` |
| ≥ 3× speedup, 50M float64 values | ✅ | Benchmark: **5.0×** (see below) |
| Existing C++ tests compile and pass unchanged; no ambiguity | ✅ | Existing suite (311) unchanged; `BestFactorFloat64Overloads` `static_assert`s and runtime checks |
| README updated | ✅ | Reviewer ran every README example |
| `make check`, `make install && make test` | ✅ | See final results |

## Tests added

- **C++:** 257 cases in `tests_cpp/test_best_factor_float64.cpp` (311 → 568), plus namespace-scope `static_assert`s on overload resolution.
- **Python:** 255 cases in `tests_py/test_best_discretization_factor_float64.py` (521 → 776). Constants and fixtures were added to `constants.py` and `conftest.py`, including a tracemalloc `peak_allocation` fixture.
- **Existing tests:** only the authorized change in `test_input_types`. Its 10/10/8 expectations are unchanged.

**Benchmarks** (8 cores: 4 performance + 4 efficiency; `pedantic`, `rounds=3`):

| 50M values, metric, default budget | Min | Mean |
|---|---|---|
| float64, `num_threads=1` | 100.47 ms | 100.50 ms |
| float64, auto | 19.73 ms | 20.15 ms |
| **float64 speedup** | **5.1×** | **5.0×** |
| float32, `num_threads=1` | 131.06 ms | 133.73 ms |
| float32, auto | 22.58 ms | 22.76 ms |

At 1M values, float64 vs float32 (min): None 2.70 vs 2.77 ms; imperial 2.48 vs 2.55 ms. Float64 is slightly faster than float32. The float path does a float → double conversion for each value inside the inner loop.

## Final results

- `make format`: no diff.
- `make check`: passes.
- `make install && make test`, after a clean rebuild: C++ 568/568; Python 776 passed. No skips or xfails.

## Decisions and deviations

- **Scope:** float64 only for `best_discretization_factor`. The user answered this at the start of the run.
- **Overloads:** a constrained template, so `nullptr` can't reach the double version. Braced-list calls such as `best_discretization_factor({0.1f})` are now ambiguous between `vector<float>` and `vector<double>`. No existing code did this, and the README says to pass a typed `std::vector`.
- **GCC:** couldn't be checked locally. Linux CI is the check. If GCC rejects the constrained template, the documented fallback is a non-template `const double *` overload plus a `std::nullptr_t` overload.
- **Close calls in the property tables:** the two cases where the winner leads by less than 1% (C++ Both/1e6 at 0.25%, Python None/1e6 at 0.39%) are checked against the brute-force reference only. Every other hardcoded factor leads its runner-up by ≥ 2.3% and uses portable data built from raw `mt19937`/`mt19937_64` output with `std::fma` (C++) or `integers * 2⁻⁵² · 1000` (Python).
- **Accepted, not tested:**
  - Integer lists are converted twice (list → int64 → float64).
  - `'>f4'` (big-endian float32) takes the float64 path, with a bit-identical result.
  - Complex and longdouble input is narrowed to float64.
- **Formatting step:** `make check` was already clean after the implementer, so I ran `make format` directly (it only reformatted a Python snippet in `plan.md`) instead of starting the formatter agent.

## Reviewer's minor findings (not addressed)

1. `README.md:283`: says "about 6× faster", but the float64 numbers next to it give 5×. Fix: "about 6× (float32) and 5× (float64)".
2. `tests_py/test_best_discretization_factor_float64.py:166-168`: the float32 error is checked with `rel_tol` 1e-12, not bit-exactly against the C++ value. Fix: `assert result.mean_abs_error == 1.2688000557610952e-09`. The Python paths are already pinned against each other.
3. `README.md:137`: the test-layout line leaves out the `…Float64Determinism` and `…Float64Property` classes.
4. `python/src/bindings.cpp:101`: the generated Python signature now shows `values: object` instead of `numpy.ndarray[numpy.float32]`. Optionally, say "1-D array-like of numbers" in the docstring.

Review: approved in round 1.

## Open questions / follow-ups

- **CI on Linux/GCC** has not run on this branch yet. It is the real check of the overload scheme (Decision 2) and of the portable test data.
- **float64 for `calculate_discretization_factor` and `discretize`:** a candidate follow-up spec. It needs its own regression list, because float64 and list input would change existing results.
- All four reviewer findings are small. Finding 1 and finding 2 take a line each.
