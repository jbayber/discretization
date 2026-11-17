#pragma once

#include <cstddef>
#include <vector>

namespace discretization {

// Candidate factor families.
//
// - Metric:   1, 10, 100, ..., 10^9.
// - Imperial: every 2^a * 3^b <= INT32_MAX (328 numbers), ascending:
//             1, 2, 3, 4, 6, 8, 9, 12, 16, 18, 24, ... (binary inch fractions,
//             12 in/ft, 3 ft/yd, 36 in/yd, 16 oz/lb).
enum class UnitSystem { Metric, Imperial };

// Returns the smallest candidate factor `f` of `unit_system` that "fits"
// `values`: for every value `v` (as double), with `q = round(v * f)` (halfway
// cases away from zero), `|q / f - v| <= epsilon`. Empty input returns 1.
//
// Throws std::invalid_argument if `epsilon` is NaN or <= 0, or any value is
// NaN or +-inf. Throws std::out_of_range if no candidate fits, or if
// `round(v * f)` overflows int32 before a candidate fits.
int calculate_discretization_factor(const std::vector<float> &values, float epsilon,
                                    UnitSystem unit_system = UnitSystem::Metric);

// Raw buffer overload: reads `size` contiguous floats starting at `values`.
// Used by the Python bindings to work directly on NumPy/pandas memory.
int calculate_discretization_factor(const float *values, std::size_t size, float epsilon,
                                    UnitSystem unit_system = UnitSystem::Metric);

// Returns `round(v * f)` for every value, where `f` is the factor returned by
// `calculate_discretization_factor` for the same arguments. Same exceptions.
std::vector<int> discretize(const std::vector<float> &values, float epsilon,
                            UnitSystem unit_system = UnitSystem::Metric);

// Raw buffer overload: reads `size` floats from `values` and writes `size`
// integers to `out`. `out` must point to at least `size` writable ints.
void discretize(const float *values, std::size_t size, float epsilon, int *out,
                UnitSystem unit_system = UnitSystem::Metric);

} // namespace discretization
