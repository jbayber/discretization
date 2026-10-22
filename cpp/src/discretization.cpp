#include "discretization/discretization.hpp"

#include <cmath>

namespace discretization {

int calculate_discretization_factor(const float * /*values*/, std::size_t /*size*/, float /*epsilon*/) {
  // Placeholder implementation: no scaling.
  return 1;
}

int calculate_discretization_factor(const std::vector<float> &values, float epsilon) {
  return calculate_discretization_factor(values.data(), values.size(), epsilon);
}

void discretize(const float *values, std::size_t size, float /*epsilon*/, int *out) {
  // Placeholder implementation: round to nearest, halfway cases away from zero.
  for (std::size_t i = 0; i < size; ++i) {
    out[i] = static_cast<int>(std::lround(values[i]));
  }
}

std::vector<int> discretize(const std::vector<float> &values, float epsilon) {
  std::vector<int> out(values.size());
  discretize(values.data(), values.size(), epsilon, out.data());
  return out;
}

} // namespace discretization
