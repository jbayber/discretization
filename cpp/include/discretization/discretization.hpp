#pragma once

#include <cstddef>
#include <vector>

namespace discretization {

// Returns the factor by which `values` must be scaled so that discretizing
// them keeps an error below `epsilon`.
int calculate_discretization_factor(const std::vector<float> &values, float epsilon);

// Raw buffer overload: reads `size` contiguous floats starting at `values`.
// Used by the Python bindings to work directly on NumPy/pandas memory.
int calculate_discretization_factor(const float *values, std::size_t size, float epsilon);

// Returns `values` discretized to integers.
std::vector<int> discretize(const std::vector<float> &values, float epsilon);

// Raw buffer overload: reads `size` floats from `values` and writes `size`
// integers to `out`. `out` must point to at least `size` writable ints.
void discretize(const float *values, std::size_t size, float epsilon, int *out);

} // namespace discretization
