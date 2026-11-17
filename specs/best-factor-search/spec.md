# Best discretization factor search (minimum mean error, parallel)

## Goal

Add an algorithm that calculates the best factor to use, based on the input floats it receives. The idea is that the factor is the one that minimizes the error over all the floats it receives. The algorithm has to be as fast as possible, and it must also run in parallel, because the list of floats can become extremely big.

In concrete terms: a new function `best_discretization_factor` searches the metric and/or imperial candidate factors up to a **factor budget** (`max_factor`). It returns the factor with the lowest **mean absolute error**, together with that error and the unit system the factor comes from. A budget is needed because a finer grid never has a larger error, so without one the "best" factor would always be the largest. On large inputs the search splits the data across threads with `std::thread`.

**Depends on** [specs/unit-system-factors](../unit-system-factors/spec.md): it reuses that spec's `UnitSystem` enum and its two candidate lists (metric `10^k`, `k = 0..9`, and imperial `2^a · 3^b ≤ INT32_MAX`). That spec must be implemented first. `/implement-spec` stops at step 1 if `UnitSystem` doesn't exist yet.

## Definitions

For a float32 input `v` (converted to `double`) and a candidate factor `f`:

- `q = round(v · f)` in `double`, with halfway cases rounded away from zero (same as `discretize`).
- `err(f) = (1/n) · Σ |q / f − v|`, the mean absolute error, computed in `double`.
- **Candidates**: the chosen system's list, or, when no system is given, the sorted union of both lists without duplicates (`1` appears once). Only candidates with `f ≤ max_factor` **and** `|round(v · f)| ≤ INT32_MAX` for every value are considered. Since `max|v| · f` grows with `f`, the overflow limit is a cutoff in the sorted list.
- **Result**: the candidate with the smallest `err(f)`. On an exact tie (`==` on the `double` errors), the **smallest** factor wins.
- `unit_system` in the result is the list the winning factor comes from. If the factor is in both lists (only `1`), report `Metric`.

## End goal / acceptance criteria

Selection (each line gives values, system, `max_factor` and the expected `factor`; errors are checked to within 1e-12 relative):

- [ ] `{0.125}`, Metric, 100 → factor `100`, error `0.005`
- [ ] `{0.125}`, Metric, default budget → `1000`, error `0`
- [ ] `{0.5, 0.25}`, Metric, default → `100`, error `0` (tie with 1000, 10⁴ … resolved to the smallest)
- [ ] `{0.1, 2.5, -3.7}`, Metric, 1000 → `10` (100 and 1000 tie exactly, so the smallest wins)
- [ ] `{2.5}`, Metric, 1 → `1`, error `0.5`
- [ ] `{3, -7, 0}`, Metric, default → `1`, error `0`
- [ ] `{0.3125}`, Imperial, 1000 → `16`, error `0`
- [ ] `{1/3}`, Imperial, 100 → `3`
- [ ] `{1/3, 0.25}`, Imperial, 100 → `12`
- [ ] `{7/12}`, Imperial, 100 → `12`
- [ ] `{0.4, 1.6, -2.5}`, Imperial, 10 → `8`, error `≈ 0.0166667`; the same input with Metric, 10 → `10`
- [ ] Both systems (no `unit_system`): `{1/3}`, 100 → `3`, Imperial; `{0.3}`, 100 → `10`, Metric; `{0.3125}`, default → `16`, Imperial
- [ ] Both systems, `{0.3}`, default budget → `16777216` (2²⁴, Imperial), error `0`. This documents that float32 values are dyadic, so with an unlimited budget the exact representation wins.
- [ ] `{1e9}`, Metric, default → `1` (larger factors overflow `int32`, so they're excluded rather than raising an error)

Errors and edge cases:

- [ ] Empty input → factor `1`, error `0`, `Metric`
- [ ] Any NaN or ±inf value → C++ `std::invalid_argument`, Python `ValueError`
- [ ] `max_factor < 1`, or `num_threads < 0` → `std::invalid_argument`, Python `ValueError`
- [ ] No candidate is considered because `|round(v)| > INT32_MAX` already at `f = 1` (`{3e9}`) → `std::out_of_range`, Python `ValueError`

Parallelism and determinism:

- [ ] The result (factor, and the error **bit-identical**) is the same for `num_threads ∈ {0 (auto), 1, 2, 3, 8, 64}` on 1M random values (fixed seed, uniform in [−1000, 1000]), for each system
- [ ] With `num_threads = 0` (auto), inputs below `100 000` values run on the calling thread and spawn no threads
- [ ] Property: on 10 000 random values (fixed seed) for budgets `{10, 1000, 10⁶}` and each system, the result equals a brute-force reference (a straightforward serial loop over all candidates, written in the test)

Performance (measured by the benchmarks and recorded in `report.md`; not an asserted test, because timing on CI is unreliable):

- [ ] On 50M float32 values (fixed seed, uniform in [−1e6, 1e6]), Metric, default budget: auto threads is **≥ 3×** faster than `num_threads = 1` on this 8-core machine
- [ ] Single pass: each chunk of data is read once and accumulates the errors of **all** candidates, rather than doing one pass per candidate

Docs:

- [ ] README API section documents the function, the result type, the budget, the tie rule, threading, and the "an unlimited budget favors exact float representation" note
- [ ] `make check` and `make install && make test` pass

## API

C++ (`cpp/include/discretization/discretization.hpp`):

```cpp
struct FactorResult {
  int factor;
  double mean_abs_error;
  UnitSystem unit_system;
};

// unit_system: std::nullopt = search both lists.
// max_factor: upper bound on the factor (inclusive); the int32 overflow cutoff still applies.
// num_threads: 0 = auto (serial below 100'000 values, else std::thread::hardware_concurrency()).
FactorResult best_discretization_factor(const std::vector<float> &values,
                                        std::optional<UnitSystem> unit_system = std::nullopt,
                                        int max_factor = std::numeric_limits<int>::max(),
                                        int num_threads = 0);
FactorResult best_discretization_factor(const float *values, std::size_t size,
                                        std::optional<UnitSystem> unit_system = std::nullopt,
                                        int max_factor = std::numeric_limits<int>::max(),
                                        int num_threads = 0);
```

The vector version delegates to the raw-buffer version. The existing functions don't change.

Python:

```python
best_discretization_factor(values, unit_system=None, max_factor=2**31 - 1, num_threads=0) -> FactorResult
# FactorResult: read-only attributes .factor (int), .mean_abs_error (float), .unit_system (UnitSystem)
```

- `unit_system` takes `None`, `"metric"`, `"imperial"` (case-insensitive) or the `UnitSystem` enum, as in unit-system-factors. Anything else raises `ValueError`.
- Bind `FactorResult` as a read-only class with a readable `__repr__`. Export `best_discretization_factor` and `FactorResult` in `__init__.py` / `__all__`.
- Keep the existing conventions: zero copy for contiguous float32, `ValueError` for input that isn't 1-D, the GIL released around the C++ call, and `std::out_of_range` translated to `ValueError`.

## Required tests

- [x] **C++ unit tests** (GoogleTest, `tests_cpp/test_best_factor.cpp`): every selection, edge-case and error criterion, through both the vector and raw-buffer versions, comparing `factor`, `unit_system` and `mean_abs_error`.
- [x] **Python unit tests** (`tests_py/test_best_discretization_factor.py`, MAGE style): the same examples, parametrized; `unit_system` given as `None`, a string and the enum; the `FactorResult` attributes and `repr`; rejection of a bad `unit_system`, `max_factor` or `num_threads`.
- [x] **Edge cases**: empty input; a single value; all zeros; negative values; NaN/±inf; `max_factor = 1`; a `max_factor` that isn't itself a candidate (for example `50` with Metric → candidates `1, 10`); the overflow cutoff (`{1e9}`) and overflow at `f = 1` (`{3e9}`).
- [x] **Property / accuracy tests**: compare against a brute-force serial reference on 10k random values (fixed seed), for 3 budgets × 3 system choices. Check determinism across `num_threads ∈ {0, 1, 2, 3, 8, 64}` on 1M values, with the error compared with `==`. Both in C++; Python covers at least the determinism check.
- [x] **Python input types**: list, float32, float64, a strided view, a pandas Series and a DataFrame column all give the same result.
- [x] **Benchmarks** (`TestBestDiscretizationFactorBenchmark`): `large_values` (1M) for each system choice with auto threads; 50M values (fixed seed) with Metric, comparing `num_threads=1` against auto. Record the times and the speedup in the report. The target is ≥ 3× (see acceptance criteria).
- [ ] **Regression tests**: none needed. Existing functions and tests are untouched.

## Constraints

- **No new dependencies.** Use C++20 `std::thread` only (link `Threads::Threads` via `find_package(Threads)` in CMake if needed, which is part of the toolchain). No OpenMP, TBB or `std::execution`.
- **Determinism:** split the input into **fixed-size chunks** (for example 65 536 values), independent of the thread count. Compute one partial sum per chunk and candidate, then add the partial sums in chunk order. This keeps the error bit-identical for any `num_threads`. Threads take chunks, either by static striping or from an atomic counter. The order of the final reduction must not depend on which thread did which chunk.
- **Single pass:** before the main pass, compute `max|v|` (parallel, same chunking) to find the overflow cutoff and validate NaN/inf. Then do one pass over the data that accumulates the errors of all considered candidates per chunk. The inner loop over candidates must not branch on data in a way that blocks auto-vectorization. Precompute `1/f` as a `double`.
- Memory: O(chunks × candidates) doubles for the partial sums. Nothing proportional to `n` beyond that.
- `num_threads` greater than the number of chunks is capped to the number of chunks. A thread count that isn't a power of two must work.
- Exceptions must not escape worker threads. Validate before spawning, or capture and rethrow on the calling thread.
- Keep the C++ core free of Python.

## Out of scope

- Changing `calculate_discretization_factor` or `discretize`, or parallelizing them.
- Other error metrics (max, RMS), a tie tolerance, or an error-plus-size trade-off.
- Integer factors outside the metric and imperial lists.
- SIMD intrinsics (rely on auto-vectorization), and GPU.

## Delivery

- [x] Commit on a feature branch `feature/best-factor-search`
- [ ] Push the branch and open a pull request

## Open questions

Resolved with the user while drafting:
- "Best" means the lowest error among candidates `≤ max_factor`, with the smallest factor winning ties.
- The error is the mean absolute error.
- Candidates reuse the unit-system lists, and both are searched when no system is given.
- Parallelism uses `std::thread`, automatic above 100k values, with a ≥ 3× speedup target on 50M values.

Remaining (non-blocking):
- Because of float32 noise, with a large budget Metric usually picks a large factor (for example, `{0.1, 2.5, -3.7}` with no budget → `10^8`) for error gains of about 1e-9. A `tie_tolerance` parameter (errors within it count as ties) would make the result more practical. It's left for a follow-up spec.
- The auto threshold (100k) and the chunk size are starting values. The implementer may tune them from the benchmark and must report the final values.
