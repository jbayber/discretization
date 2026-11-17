#include "discretization/discretization.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>

namespace discretization {

namespace {

constexpr std::int64_t kInt32Max = std::numeric_limits<std::int32_t>::max();

constexpr std::array<int, 10> kMetricCandidates = {
    1, 10, 100, 1'000, 10'000, 100'000, 1'000'000, 10'000'000, 100'000'000, 1'000'000'000};

constexpr std::size_t CountImperialCandidates() {
  std::size_t count = 0;
  for (std::int64_t p2 = 1; p2 <= kInt32Max; p2 *= 2) {
    for (std::int64_t p = p2; p <= kInt32Max; p *= 3) {
      ++count;
    }
  }
  return count;
}

constexpr std::size_t kImperialCount = CountImperialCandidates();

// All 2^a * 3^b <= INT32_MAX, sorted ascending. Uses a hand-written insertion
// sort so it does not depend on constexpr std::sort support.
constexpr std::array<int, kImperialCount> BuildImperialCandidates() {
  std::array<int, kImperialCount> out{};
  std::size_t n = 0;
  for (std::int64_t p2 = 1; p2 <= kInt32Max; p2 *= 2) {
    for (std::int64_t p = p2; p <= kInt32Max; p *= 3) {
      out[n++] = static_cast<int>(p);
    }
  }
  for (std::size_t i = 1; i < n; ++i) {
    const int key = out[i];
    std::size_t j = i;
    while (j > 0 && out[j - 1] > key) {
      out[j] = out[j - 1];
      --j;
    }
    out[j] = key;
  }
  return out;
}

constexpr std::array<int, kImperialCount> kImperialCandidates = BuildImperialCandidates();

constexpr bool IsStrictlyAscending(const auto &arr) {
  for (std::size_t i = 1; i < arr.size(); ++i) {
    if (arr[i - 1] >= arr[i])
      return false;
  }
  return true;
}

static_assert(kImperialCount == 328);
static_assert(kImperialCandidates.front() == 1);
static_assert(kImperialCandidates.back() == 2066242608);
static_assert(kImperialCandidates[19] == 96);
static_assert(IsStrictlyAscending(kImperialCandidates));
static_assert(IsStrictlyAscending(kMetricCandidates));
static_assert(kMetricCandidates.back() == 1'000'000'000);

std::span<const int> CandidatesFor(UnitSystem unit_system) {
  switch (unit_system) {
  case UnitSystem::Metric:
    return kMetricCandidates;
  case UnitSystem::Imperial:
    return kImperialCandidates;
  }
  throw std::invalid_argument("unknown unit system");
}

void Validate(const float *values, std::size_t size, float epsilon) {
  if (!(epsilon > 0.0f))
    throw std::invalid_argument("epsilon must be a positive number");
  for (std::size_t i = 0; i < size; ++i) {
    if (!std::isfinite(values[i]))
      throw std::invalid_argument("values must be finite (no NaN or inf)");
  }
}

// round(x) with halfway cases away from zero; throws if it does not fit int32.
// Uses std::round on double rather than std::lround (long is 32-bit on Windows).
double RoundChecked(double x) {
  const double q = std::round(x);
  if (q < static_cast<double>(std::numeric_limits<std::int32_t>::min()) ||
      q > static_cast<double>(std::numeric_limits<std::int32_t>::max())) {
    throw std::out_of_range("discretized value overflows int32");
  }
  return q;
}

} // namespace

int calculate_discretization_factor(const float *values, std::size_t size, float epsilon, UnitSystem unit_system) {
  const auto candidates = CandidatesFor(unit_system);
  Validate(values, size, epsilon);
  if (size == 0)
    return 1;

  const double eps = static_cast<double>(epsilon);
  for (const int candidate : candidates) {
    const double f = static_cast<double>(candidate);
    bool fits = true;
    for (std::size_t i = 0; i < size; ++i) {
      const double v = static_cast<double>(values[i]);
      const double q = RoundChecked(v * f); // overflow: larger factors only make it worse
      if (std::abs(q / f - v) > eps) {
        fits = false;
        break;
      }
    }
    if (fits)
      return candidate;
  }
  throw std::out_of_range("no candidate discretization factor fits the values within epsilon");
}

int calculate_discretization_factor(const std::vector<float> &values, float epsilon, UnitSystem unit_system) {
  return calculate_discretization_factor(values.data(), values.size(), epsilon, unit_system);
}

void discretize(const float *values, std::size_t size, float epsilon, int *out, UnitSystem unit_system) {
  const int factor = calculate_discretization_factor(values, size, epsilon, unit_system);
  const double f = static_cast<double>(factor);
  for (std::size_t i = 0; i < size; ++i) {
    out[i] = static_cast<int>(std::round(static_cast<double>(values[i]) * f));
  }
}

std::vector<int> discretize(const std::vector<float> &values, float epsilon, UnitSystem unit_system) {
  std::vector<int> out(values.size());
  discretize(values.data(), values.size(), epsilon, out.data(), unit_system);
  return out;
}

} // namespace discretization
