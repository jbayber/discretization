# Plan: Best discretization factor search

Written by the `planner` subagent, condensed by the orchestrator. Orchestrator decisions on the planner's risks are at the end.

## Summary

Add `best_discretization_factor`, which returns a `FactorResult {factor, mean_abs_error, unit_system}`. It searches the metric list, the imperial list, or both. Both lists together form a deduplicated `constexpr` union of 337 entries. Only candidates `≤ max_factor` and below the int32 overflow cutoff are considered. Everything lives in `cpp/src/discretization.cpp` and reuses `kMetricCandidates` / `kImperialCandidates`.

The data is split into fixed 65 536-value chunks and processed in two passes:
1. A pre-pass computes `max|v|` and checks for NaN/inf.
2. A single main pass gives each chunk one partial sum per candidate.

Partial sums are added in chunk order, and the work runs on `std::thread`s or on the calling thread.

**Planner re-verification:** `{0.1, 2.5, -3.7}` with Metric and a budget of 1000 ties exactly at 10, 100 and 1000 (error 1.6391277252170156e-08). The result is still 10.

## API

```cpp
struct FactorResult { int factor; double mean_abs_error; UnitSystem unit_system; };

FactorResult best_discretization_factor(const std::vector<float> &values,
                                        std::optional<UnitSystem> unit_system = std::nullopt,
                                        int max_factor = std::numeric_limits<int>::max(),
                                        int num_threads = 0);
FactorResult best_discretization_factor(const float *values, std::size_t size,
                                        std::optional<UnitSystem> unit_system = std::nullopt,
                                        int max_factor = std::numeric_limits<int>::max(),
                                        int num_threads = 0);
```

- `std::invalid_argument`: `max_factor < 1`, `num_threads < 0`, an invalid enum value, or NaN/±inf.
- `std::out_of_range`: no candidate survives the overflow cutoff.

**Internal test-only header** `cpp/src/discretization_internal.hpp` (namespace `discretization::detail`, not public):
- `kChunkSize = 65'536`
- `kAutoParallelThreshold = 100'000`
- `ResolveThreadCount(size, num_threads, hw)`
- `SpawnedThreadCount()`: a global atomic counter of spawned threads

**Python**: `best_discretization_factor(values, unit_system=None, max_factor=2**31 - 1, num_threads=0) -> FactorResult`.
- `FactorResult` is read-only, has no Python constructor, and has the `repr` `FactorResult(factor=16, mean_abs_error=0.0, unit_system=UnitSystem.IMPERIAL)`.
- `unit_system` accepts `None`, a string or a `UnitSystem`, using the existing `parse_unit_system`.
- `max_factor` is bound as int64: below 1 raises `ValueError`, and values above INT32_MAX are clamped.
- `out_of_range` is translated to `ValueError`.

## Files

| File | Change |
|---|---|
| `cpp/include/discretization/discretization.hpp` | `FactorResult`, declarations, docs (`<optional>`, `<limits>`) |
| `cpp/src/discretization.cpp` | union table, chunked helper, spawn counter, `ResolveThreadCount`, the function |
| `cpp/src/discretization_internal.hpp` (new) | `detail` declarations |
| `CMakeLists.txt` | `find_package(Threads REQUIRED)`, link `Threads::Threads` PUBLIC, `cpp/src` include directory for `discretization_tests` |
| `python/src/bindings.cpp` | `py::class_<FactorResult>`, wrapper, `m.def` |
| `python/src/discretization/__init__.py` | export `FactorResult`, `best_discretization_factor` |
| `tests_cpp/test_best_factor.cpp` (new) | C++ tests |
| `tests_py/test_best_discretization_factor.py` (new) | Python tests |
| `tests_py/constants.py`, `conftest.py` | `BEST_FACTOR_*` constants, `determinism_values` (1M) and `huge_values` (50M) fixtures; additions only |
| `README.md` | API section and test layout |

## Test plan

Error tolerance: `==` when the expected value is 0, otherwise a relative tolerance of 1e-12.

### Selection table (C++ `BestFactorSelection` and Python `test_examples`)

| name | values | sys | max | factor | error | unit_system |
|---|---|---|---|---|---|---|
| EighthBudget100 | {0.125} | M | 100 | 100 | 0.005 | Metric |
| EighthDefault | {0.125} | M | D | 1000 | 0 | Metric |
| HalfQuarterTieSmallest | {0.5,0.25} | M | D | 100 | 0 | Metric |
| MixedExactTie | {0.1,2.5,-3.7} | M | 1000 | 10 | 1.6391277252170156e-08 | Metric |
| BudgetOne | {2.5} | M | 1 | 1 | 0.5 | Metric |
| Integers | {3,-7,0} | M | D | 1 | 0 | Metric |
| FiveSixteenths | {0.3125} | I | 1000 | 16 | 0 | Imperial |
| Third | {1/3f} | I | 100 | 3 | 9.934107481068821e-09 | Imperial |
| ThirdQuarter | {1/3f,0.25} | I | 100 | 12 | 4.967053740534411e-09 | Imperial |
| SevenTwelfths | {7/12f} | I | 100 | 12 | 1.9868214962137642e-08 | Imperial |
| ValuesImperial10 | {0.4,1.6,-2.5} | I | 10 | 8 | 0.01666666070620219 | Imperial |
| ValuesMetric10 | {0.4,1.6,-2.5} | M | 10 | 10 | 9.93410742555767e-09 | Metric |
| BothThird | {1/3f} | B | 100 | 3 | 9.934107481068821e-09 | Imperial |
| BothPointThree | {0.3} | B | 100 | 10 | 1.1920928966180355e-08 | Metric |
| BothFiveSixteenths | {0.3125} | B | D | 16 | 0 | Imperial |
| BothUnlimitedDyadic | {0.3} | B | D | 16777216 | 0 | Imperial |
| OverflowCutoff | {1e9} | M | D | 1 | 0 | Metric |

### Edge cases

- Empty input with every system, and nullptr/0 on the raw version → {1, 0, Metric}.
- `{-0.125}` M → 1000.
- All zeros → {1, 0, Metric} for every system.
- `{-0.3125}` I → 16.
- `max_factor = 1` on `{0.4, 1.6, -2.5}` → {1, 0.43333332737286884}.
- `max_factor = 50` on `{0.125}` → {10, 0.024999999999999994}.
- `{1e9, 0.5}`: M → {1, 0.25}; I → {2, 0}; B → {2, 0, Imperial}.
- `{1e9}` I → {1, 0, Metric}.
- `{2147483520.0f}` → 1.

### Errors

- NaN or ±inf, for each system → `invalid_argument`. `{3e9, NaN}` → `invalid_argument`, because NaN is checked first.
- `max_factor` of 0, −1 or INT_MIN, and `num_threads = −1` → `invalid_argument`, also on empty input.
- `{3e9}` with M, I or B, and with `max_factor = 1` → `out_of_range`.
- `{±2147483648.0f}` → `out_of_range`.
- `static_cast<UnitSystem>(42)` → `invalid_argument`.

### Threads (C++, internal header)

- `ResolveThreadCount` cases:
  - below the threshold → 1;
  - exactly at the threshold → `min(8, ceil(100000/kChunkSize))`;
  - capped to the number of chunks (64 threads on 1M values → 16);
  - `hw = 0` → 1.
- `NoSpawnBelowThreshold`: 99 999 values with auto threads → the spawn-counter delta is 0. 1M values with `num_threads = 1` → delta 0.
- `SpawnsAboveThreshold`: 1M values with `num_threads = 3` → delta 2.
- `static_assert(kChunkSize >= 10'000)`.

### Determinism

- C++: 1M values, `mt19937{12345}`, uniform in [−1000, 1000], for M, I and B. For `num_threads` of 0, 1, 2, 3, 8 and 64, the factor and unit system are identical and the error matches through `bit_cast`.
- Python: the same check with `default_rng(7)`. Also, 99 999 values give the same result with auto and with 1 thread.

### Property

- C++: 10k values (fixed seed), budgets {10, 1000, 10⁶} × M/I/B. Compare against a serial reference, including exact error equality. This is valid because 10k < `kChunkSize`.
- Python: the `property_values` fixture (seed 42) against a NumPy reference, with `rel_tol` 1e-12 on the error and a check that the result is the minimum.
- Expected factors:

  | System | 10 | 1000 | 10⁶ |
  |---|---|---|---|
  | M | 10 | 1000 | 1000000 |
  | I | 9 | 972 | 524288 |
  | B | 10 | 1000 | 524288 (Imperial) |

### Python-only tests

- `TestFactorResult`: attribute types, the `repr`, `AttributeError` when setting an attribute, `TypeError` from the constructor, and the names exported in `__all__`.
- Errors: non-finite values, overflow, `max_factor` 0 or −1, `num_threads` −1 or −8, bad `unit_system` values (`"cubits"`, `""`, `" metric"`, `0`, `1`, `1.5`), and `max_factor = 10**12` being clamped.
- Input types: list, float32, float64, strided view, Series, DataFrame column. All give the same result.

### Benchmarks

- `test_large_values[None|metric|imperial]`.
- `test_huge_values[threads=1|auto]`: 50M values, `default_rng(1)`, uniform in [−1e6, 1e6], `pedantic` with `rounds=3`. Expect factor 1000, Metric. The speedup is recorded in the report, not asserted.

## Implementation steps

1. **CMake.** Add Threads and the test include directory.
2. **`kBothCandidates`.** A `constexpr` merge of the two lists without duplicates (337 entries), with `static_assert`s. `CandidatesFor(std::optional<UnitSystem>)` picks the list.
3. **Validation.** In this order, before any thread starts: `max_factor`, `num_threads`, the unit system, then the empty-input shortcut.
4. **`ForEachChunk(num_chunks, threads, body)`.**
   - `threads <= 1` → serial, nothing spawned.
   - Otherwise an atomic chunk counter, `threads − 1` spawned threads (each one counted), and the calling thread also does work.
   - An RAII join. If thread creation fails with `system_error`, stop spawning and keep going.
   - Exceptions are captured as an `exception_ptr` and rethrown after the join.
5. **`ResolveThreadCount`.**
   - `chunks = ceil(size / kChunkSize)`.
   - `req = num_threads > 0 ? num_threads : (size < threshold ? 1 : max(hw, 1))`.
   - Return `max(1, min(req, chunks))`.
6. **Pre-pass.** Per-chunk `max|v|` (branch-free) plus a "bad value" flag. Throw `invalid_argument` after the join if any chunk is bad.
7. **Cutoff.** Walk the candidates in ascending order and keep `f` while `f ≤ max_factor` and `round(m·f) ≤ INT32_MAX`. If none survive → `out_of_range`.
8. **Main pass.**
   - Per chunk: `for i in chunk: x = v[i]; for k in K: acc[k] += |round(x·f_k) / f_k − x|`.
   - Candidates are the inner loop. Sums run in index order within a chunk. No `-ffast-math`.
   - Store the partial sums in `partial[c·K + k]`.
   - Use true division (see Decisions).
9. **Reduction on the calling thread.** For each candidate, add the partial sums in chunk order, then divide by n. The first strict minimum wins, so ties go to the smallest factor. `unit_system` is Metric if the factor is in the metric list, else Imperial.
10. **Vector version** delegates to the raw version.
11. **Bindings.**
    - `py::class_<FactorResult>` with `def_readonly` ×3 and `__repr__`, registered before the function.
    - Wrapper order: ndim check → parse the unit system (`None` → nullopt) → check and clamp `max_factor` → release the GIL → call → `out_of_range` becomes `ValueError`.
12. **`__init__.py`.** Exports.
13. **README.** API, the definition of `err(f)`, candidates, budget and cutoff, the tie rule and unit-system attribution, errors, threading details, the note on unlimited budgets (`[0.3]` → 16777216), and a Python example.
14. Run `make format`, `make check`, `make install`, `make test`, then the benchmarks. Tune `kChunkSize` and the threshold if 3× is missed, and report the final values.

## Decisions on the planner's risks (orchestrator)

1. **Factor 1 under Imperial reports Metric.** The spec's rule is unconditional, so `{3,-7,0}`, `{1e9}` and `{}` with Imperial report Metric. **Accepted as planned.**
2. **True division `q / f` instead of multiplying by a precomputed `1/f`.** The spec's Constraints say "precompute 1/f", but its Definitions say `q / f`. Multiplying by `1/f` changes the last bit, which flips exact ties and breaks equality with the reference. **The Definitions win. This is recorded as a deviation.**
3. **Sums are sequential within each chunk, and chunks are reduced in order.** The implementer must not vectorize the reduction across values.
4. **Python `max_factor` above INT32_MAX is clamped.** **Accepted.**
5. **Validation order:** arguments are checked before the empty-input shortcut, and NaN before overflow. **Accepted.**
6. **The C++ tests build at -O0**, so the determinism tests take a few seconds. This is acceptable.
7. **The spawn counter lives in an internal header** and touches no public API. **Accepted.**
8. **The 3× speedup isn't guaranteed.** This machine has 4 performance cores and 4 efficiency cores, and the metric search is bandwidth-bound with only 4 candidates. The speedup is recorded, not asserted.
9. **pytest timeout is 60 s.** Keep `rounds=3` for the 50M benchmark.
