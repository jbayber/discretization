# Report: Best discretization factor search

**Status:** done. Branch `feature/best-factor-search`, not pushed (the spec doesn't ask for a PR).

## Summary

New function `best_discretization_factor(values, unit_system, max_factor, num_threads)` (C++ vector and raw-buffer versions, plus Python). It returns `FactorResult {factor, mean_abs_error, unit_system}`.

- **What it picks:** among the metric list, the imperial list, or both (a 337-entry union built at compile time), it keeps candidates `f ≤ max_factor` that don't overflow `int32` for the data. It returns the one with the lowest mean absolute error `mean |round(v·f)/f − v|`. Exact ties go to the smallest factor. A factor in the metric list is reported as Metric (factor 1 included), otherwise Imperial.
- **Parallelism:** the input is split into fixed chunks of 65 536 values.
  - A pre-pass computes `max|v|` and rejects NaN/inf.
  - One main pass accumulates the errors of all candidates per chunk.
  - Chunk sums are added in chunk order on the calling thread, so the result is bit-identical for any thread count.
  - `num_threads = 0` (auto) runs serially below 100 000 values and uses `hardware_concurrency()` threads above that. The count is capped to the number of chunks.
  - Threading uses `std::thread` only (CMake `Threads::Threads`). Workers don't throw, and any captured exception is rethrown on the calling thread. Threads are joined on every path.
- **Python:** `unit_system` takes `None`, `"metric"`/`"imperial"` or the `UnitSystem` enum. `FactorResult` is read-only and has a `repr`. Errors raise `ValueError`, and the GIL is released during the call.
- **README:** the API section documents the function, the result, the budget, the tie rule, threading, and the note on unlimited budgets.

## Acceptance criteria

| Criterion | | Tests (C++ / Python) |
|---|---|---|
| `{0.125}` M 100 → 100, err 0.005 | ✅ | `BestFactorSelection.*/EighthBudget100` / `test_examples` |
| `{0.125}` M default → 1000, err 0 | ✅ | `…/EighthDefault` |
| `{0.5, 0.25}` M → 100 (tie, smallest wins) | ✅ | `…/HalfQuarterTieSmallest` |
| `{0.1, 2.5, -3.7}` M 1000 → 10 (exact tie) | ✅ | `…/MixedExactTie` |
| `{2.5}` M 1 → 1, err 0.5 | ✅ | `…/BudgetOne` |
| `{3, -7, 0}` M → 1, err 0 | ✅ | `…/Integers` |
| `{0.3125}` I 1000 → 16 | ✅ | `…/FiveSixteenths` |
| `{1/3}` I 100 → 3 | ✅ | `…/Third` |
| `{1/3, 0.25}` I 100 → 12 | ✅ | `…/ThirdQuarter` |
| `{7/12}` I 100 → 12 | ✅ | `…/SevenTwelfths` |
| `{0.4, 1.6, -2.5}` I 10 → 8 / M 10 → 10 | ✅ | `…/ValuesImperial10`, `…/ValuesMetric10` |
| Both systems: `{1/3}` → 3 I; `{0.3}` 100 → 10 M; `{0.3125}` → 16 I | ✅ | `…/BothThird`, `…/BothPointThree`, `…/BothFiveSixteenths` |
| Both systems, `{0.3}` default → 16777216 (I), err 0 | ✅ | `…/BothUnlimitedDyadic` / `test_examples` |
| `{1e9}` M → 1 (overflow cutoff) | ✅ | `…/OverflowCutoff`, `BestFactorEdge.*` |
| Empty → {1, 0, Metric} | ✅ | `BestFactorEdge.EmptyInput` / `test_empty` |
| NaN / ±inf → invalid_argument / ValueError | ✅ | `BestFactorErrors.NonFiniteValues`, `ErrorsFromLargeParallelInput` / `test_non_finite_values` |
| `max_factor < 1`, `num_threads < 0` → invalid_argument / ValueError | ✅ | `BestFactorErrors.InvalidMaxFactor`, `NegativeNumThreads` / `test_invalid_max_factor`, `test_invalid_num_threads` |
| `{3e9}` → out_of_range / ValueError | ✅ | `BestFactorErrors.OverflowAtFactorOne` / `test_overflow_at_factor_one` |
| Bit-identical for `num_threads` ∈ {0, 1, 2, 3, 8, 64}, 1M values, each system | ✅ | `BestFactorDeterminism.BitIdenticalAcrossThreadCounts/*` / `TestBestDiscretizationFactorDeterminism` |
| Auto below 100 000 values → no threads spawned | ✅ | `BestFactorThreads.NoSpawnBelowThreshold`, `NoSpawnWithOneThread`, `ResolveAuto*` / `test_below_auto_threshold` |
| Matches the brute-force reference (10k values, 3 budgets × 3 system choices) | ✅ | `BestFactorProperty.MatchesBruteForceReference/*` / `TestBestDiscretizationFactorProperty` |
| ≥ 3× speedup on 50M values (auto vs 1 thread) | ✅ | Benchmark `test_huge_values`: **6.46×** (see below) |
| Single pass: all candidates per chunk | ✅ | Code review (reviewer confirmed) |
| README updated | ✅ | Reviewer ran the examples |
| `make check`, `make install && make test` | ✅ | See final results |

## Tests added

- **C++:** 122 tests in `tests_cpp/test_best_factor.cpp` (189 → 311).
- **Python:** 150 tests in `tests_py/test_best_discretization_factor.py` (371 → 521). Constants and fixtures were added to `constants.py` and `conftest.py`, and no existing lines changed.

**Benchmarks** (this machine: 8 cores, 4 performance + 4 efficiency; times in ms):

| Benchmark | Min | Mean |
|---|---|---|
| `test_huge_values[threads=1]` (50M, Metric) | 129.68 | 130.37 |
| `test_huge_values[auto]` (50M, Metric) | 20.13 | 20.19 |
| **Speedup** | **6.44×** | **6.46×** |
| `test_large_values[metric]` (1M) | 0.506 | 0.562 |
| `test_large_values[imperial]` (1M) | 2.468 | 2.480 |
| `test_large_values[None]` (1M, both lists) | 2.688 | 2.701 |

**Final tuning values:** `kChunkSize = 65 536` and `kAutoParallelThreshold = 100 000`. These are the starting values, and no tuning was needed.

## Final results

- `make format`: no diff.
- `make check`: passes.
- `make install && make test`, after a clean rebuild: C++ 311/311; Python 521 passed. No skips or xfails.

## Decisions and deviations

- **True division `q / f` instead of a precomputed `1/f`.** This deviates from the spec's Constraints. The spec's Definitions use `q / f`, and multiplying by `1/f` changes the last bit (`{0.5, 0.25}` at f=10 gives 0.024999999999999994 vs 0.025000000000000022). That can flip exact ties and break equality with the reference. Division still vectorizes, and the speedup target was met.
- **Factor 1 is reported as Metric even when Imperial was requested.** This is the spec's rule taken literally. It affects `{}`, all-integer data and `{1e9}` under Imperial.
- **Python `max_factor` above INT32_MAX is clamped** to INT32_MAX, meaning "no limit".
- **Validation order:** `max_factor`, then `num_threads`, then the unit system, then the empty-input shortcut. NaN/inf are reported before overflow.
- **Spawn observability:** a global spawn counter and `ResolveThreadCount` live in the internal header `cpp/src/discretization_internal.hpp`. The header is used only by the tests and is not part of the public API.
- **Formatting step:** `make check` was already clean after the implementer, so I ran `make format` directly (no diff) instead of starting the formatter agent.

## Post-delivery fix: CI failure on Linux

`BestFactorProperty.MatchesBruteForceReference/{Imperial,Both}Budget1000000` failed on GitHub Actions (`ubuntu-latest`) and passed on macOS. Two causes made the C++ test data platform-dependent, while the expected-factor table was hardcoded from macOS data:

1. `std::uniform_real_distribution<float>` is implementation-defined, so libc++ and libstdc++ produce different values from the same `mt19937` seed.
2. `low + (high - low) * u` is contracted into an FMA by Apple clang on arm64 but not by GCC on x86-64. This changed 5290 of the 10 000 values.

**Fix** (`RandomValues` in `tests_cpp/test_best_factor.cpp`): build each value from the raw `mt19937` output only, as `std::fma(high - low, float(gen() >> 8) * 0x1p-24f, low)`. The raw output is fixed by the standard, and the explicit `fma` is exactly rounded everywhere.

The new data changes the budget-10⁶ winner for Imperial and Both from 524288 to **786432** (3·2¹⁸), which leads the runner-up by 17.5%. An independent NumPy reference on the same `mt19937` stream gives all 9 expected factors. The remaining 7 expectations are unchanged. The test keeps its full strength, with the hardcoded table and the exact comparison against the reference.

`tests_cpp/test_property.cpp` (from unit-system-factors) also uses `uniform_real_distribution`. It only checks properties that hold for any data, so it is portable as is.

## Reviewer's minor findings (not addressed)

1. `python/src/bindings.cpp:73-74`: `num_threads=2**40`, `max_factor=2**70` or `num_threads=1.5` raise `TypeError` (pybind11 argument conversion) instead of `ValueError`. The negative values the spec calls out do raise `ValueError`. Fix: take both as `py::int_` and range-check them.
2. `cpp/src/discretization.cpp`: the pre-pass and the main pass each spawn and join their own threads, so a call spawns `2·(threads−1)` threads. This is negligible on large inputs but visible just above 100k values. A single pool with a barrier would remove it.
3. `tests_cpp/test_best_factor.cpp:174-188`: the non-zero expected errors in the selection table have no comment explaining that they are float32 representation errors, also checked against the brute-force reference.
4. `cpp/src/discretization.cpp`, `ForEachChunk`: `joiner.threads.clear()` after the explicit join loop duplicates what the RAII destructor does.

Review: approved in round 1.

## Open questions / follow-ups

- **`tie_tolerance`** (from the spec): with a large budget, float32 noise makes Metric choose huge factors for gains around 1e-9. For example, `{0.1, 2.5, -3.7}` with no budget returns 10⁸. A tolerance under which errors count as ties would make the result more practical. Candidate for the next spec.
- The test-writer reported a duplicate `discretize` call in `python/src/bindings.cpp`. I checked and it doesn't exist (one call at line 60), so no action is needed.
- From the previous spec, still open: `discretize`'s second pass uses an unchecked cast. This is a one-line hardening.
