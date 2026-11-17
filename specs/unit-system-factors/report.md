# Report: Unit-system discretization factors

**Status:** done. Branch `feature/unit-system-factors`, not pushed (the spec doesn't ask for a PR).

## Summary

The placeholder factor (always `1`) is replaced by a real search. For the chosen unit system, `calculate_discretization_factor` returns the smallest candidate factor `f` such that `|round(v·f)/f − v| ≤ epsilon` for every value, computed in double and rounding halfway away from zero. `discretize` returns `round(v·f)` as `int32`, using exactly that factor.

- **Metric:** `10^0 … 10^9`.
- **Imperial:** the 328 values of `2^a·3^b ≤ INT32_MAX` (1, 2, 3, 4, 6, 8, 9, 12, 16, …, 2066242608), built at compile time and checked with `static_assert`s.
- **C++:** a trailing `UnitSystem unit_system = UnitSystem::Metric` on all four signatures, so existing calls are unchanged.
- **Python:** `unit_system=` takes `"metric"` / `"imperial"` (case-insensitive) or `discretization.UnitSystem.METRIC` / `.IMPERIAL`, and is exported in `__all__`.
- **Errors:**
  - Epsilon `≤ 0` or NaN, or NaN/inf values → `std::invalid_argument`.
  - No candidate fits, or `int32` overflow → `std::out_of_range`.
  - Python raises `ValueError` for all of these, and for an unknown `unit_system`. `out_of_range` is translated in the binding.
- The README API section is updated and the placeholder note is removed.

## Acceptance criteria

| Criterion | | Tests |
|---|---|---|
| `{0.1, 2.5, -3.7}`, 0.01 → 10 | ✅ | C++ `UnitSystemExamples/Metric…/Mixed`, `CalculateDiscretizationFactor.VectorMetric`; Py `…Metric::test_examples[mixed-*]` |
| `{1.234}`: 0.001 → 1000, 0.005 → 100 | ✅ | `Fine1234`, `Coarse1234` (C++ and Py) |
| `{3, -7, 0}` → 1 | ✅ | `Integers`, `SingleInteger` |
| `{1/3}`, 1e-6 → 1 000 000 | ✅ | Metric `Third` |
| Empty → 1 / empty result (both systems) | ✅ | `Empty` (both suites), `UnitSystemEmpty.RawNullBuffers`, `EmptyVectorReturnsOne`, `Discretize.EmptyVector`, `test_empty` |
| `discretize({0.4, 1.6, -2.5}, 0.01)` → `{4, 16, -25}` | ✅ | Metric `Values`, `TestDiscretizeInputTypes` (`Constants.DISCRETIZED`) |
| Nearest at 0.5 → `{0, 1, 2, -1, -2}` | ✅ | `Discretize.VectorRoundsToNearest`, `Nearest`, `test_rounding[nearest]` |
| Halfway at 0.5 → `{1, 2, 3, -1, -3}` | ✅ | `Discretize.HalfwayRoundsAwayFromZero`, `HalfwayAwayFromZero`, `test_rounding[halfway-…]` |
| Default = Metric | ✅ | `UnitSystemDefault.*`, `DefaultMatchesExplicitMetric/*`; Py `test_default_is_metric`, `test_default_differs_from_imperial`, `test_metric_default` |
| Imperial 5/16 → 16 / `{5}` | ✅ | Imperial `FiveSixteenths` (and `NegativeFiveSixteenths`) |
| Imperial 1/3 → 3 / `{1}` | ✅ | Imperial `Third` |
| Imperial 7/12 → 12 / `{7}` | ✅ | `SevenTwelfths` |
| Imperial `{2.25, 0.5}` → 4 / `{9, 2}` | ✅ | `Quarters` |
| Imperial `{1/3, 0.25}` → 12 / `{4, 3}` | ✅ | `ThirdAndQuarter` |
| Imperial `{0.5, 0.25, 0.3125}` → 16 / `{8, 4, 5}` | ✅ | `Sixteenths` |
| Imperial `{0.1, 2.5, -3.7}` → 48 / `{5, 120, -178}` | ✅ | Imperial `Mixed` |
| Imperial `{0.4, 1.6, -2.5}` → 32 / `{13, 51, -80}` | ✅ | Imperial `Values`, `TestDiscretizeInputTypesImperial` |
| Epsilon ≤ 0 / NaN → invalid_argument / ValueError | ✅ | `ErrorsTest.Epsilon{Zero,NegativeZero,Negative,NaN,InvalidOnEmptyInput}`; Py `test_invalid_epsilon*` |
| NaN / ±inf values → invalid_argument / ValueError | ✅ | `ErrorsTest.{NaNValue,PosInfValue,NegInfValue,NonFiniteSingleValue}`; Py `test_non_finite_values` |
| No candidate fits (`{0.1}`, 1e-12, metric) → out_of_range / ValueError | ✅ | `Errors.NoCandidateFitsMetric[Default]`; Py `test_no_candidate_fits_metric` |
| `int32` overflow (`{3e9}`) → out_of_range / ValueError | ✅ | `ErrorsTest.Int32Overflow[Negative]`; Py `test_int32_overflow` |
| Unknown `unit_system` → ValueError | ✅ | `Test*UnitSystem::test_rejects_unknown_unit_system[_keyword]` |
| Property: 10k values, 3 epsilons, both systems, smallest fitting factor | ✅ | C++ `PropertyTest.*`, `PropertyCandidates.*`; Py `…Property::*` |
| Vector == raw buffer everywhere | ✅ | `*.RawMatchesVector`, `ArrayMatchesVector`, every error test calls both |
| README API section updated | ✅ | Checked by the reviewer |
| `make check`, `make install && make test` pass | ✅ | See final results |

## Tests added

- **C++:** 182 new tests (7 → 189) in `test_unit_system_factors.cpp`, `test_errors.cpp` and `test_property.cpp`. `test_discretization.cpp` has only the authorized regression edits.
- **Python:** 353 new tests (18 → 371) across `test_calculate_discretization_factor.py` and `test_discretize.py`. Shared data is in `constants.py`, and the `property_values`, `fits` and `round_half_away` fixtures are in `conftest.py`.
- **Benchmarks** (1M float32 values, ε = 0.01, mean on this machine):

| Benchmark | Mean |
|---|---|
| `calculate_discretization_factor`, metric (factor 100) | 813 µs |
| `calculate_discretization_factor`, imperial (factor 54) | 817 µs |
| `discretize`, metric, float32 no copy | 973 µs |
| `discretize`, metric, float64 converted | 1087 µs |
| `discretize`, imperial, float32 no copy | 971 µs |

The placeholder `discretize` baseline was about 285 µs. The added time comes from the factor search, which is now a real pass over the data.

## Final results

- `make format`: no diff.
- `make check`: passes (ruff format, ruff check, clang-format).
- `make install && make test`: C++ 189/189 passed; Python 371 passed. No skips, no xfails.

## Decisions and deviations

- **Epsilon is validated before the empty-input shortcut**, so `{}` with ε = 0 raises. The spec's error rule is unconditional.
- **`+inf` epsilon is accepted** and not tested.
- **Non-finite values are reported before overflow**, because validation is an upfront pass.
- **`std::round` on double is used instead of `lround`**, because `long` is 32 bits on Windows.
- **An unknown enum value is checked before epsilon.** This differs from the plan's order; no test depends on it.
- **Formatting step:** `make check` was already clean after the implementer, so I ran `make format` directly (no diff) instead of starting the formatter agent.
- **Imperial input types** are covered in a new class, `TestDiscretizeInputTypesImperial`, rather than by editing the existing parametrization. This keeps the existing tests byte-for-byte.

## Reviewer's minor findings (not addressed)

1. `cpp/src/discretization.cpp:135`: `discretize` reads the input twice and the second pass uses an unchecked `static_cast<int>`. This is only reachable if another thread changes the NumPy array while the GIL is released, which is a caller-side race. Fix: use `RoundChecked` in the write loop.
2. `README.md:233`: in the imperial Python example, the output comment sits above the call instead of after it. This is cosmetic.

Review: approved in round 1.

## Open questions / follow-ups

- `specs/best-factor-search` depends on this branch (`UnitSystem` and the candidate lists). Merge this branch first.
- Consider adopting reviewer finding 1, since it is a cheap hardening.
