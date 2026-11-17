#include "discretization/discretization.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <random>
#include <string>
#include <tuple>
#include <vector>

using discretization::calculate_discretization_factor;
using discretization::discretize;
using discretization::UnitSystem;

namespace {

constexpr std::uint32_t kSeed = 42;
constexpr std::size_t kSize = 10'000;
constexpr float kLow = -1000.0f;
constexpr float kHigh = 1000.0f;

// Reference candidate lists, built independently of the library.
std::vector<int> MetricCandidates() {
  std::vector<int> candidates;
  for (std::int64_t f = 1; f <= 1'000'000'000; f *= 10)
    candidates.push_back(static_cast<int>(f));
  return candidates;
}

std::vector<int> ImperialCandidates() {
  constexpr std::int64_t kMax = std::numeric_limits<std::int32_t>::max();
  std::vector<int> candidates;
  for (std::int64_t two = 1; two <= kMax; two *= 2) {
    for (std::int64_t f = two; f <= kMax; f *= 3)
      candidates.push_back(static_cast<int>(f));
  }
  std::sort(candidates.begin(), candidates.end());
  return candidates;
}

std::vector<int> CandidatesFor(UnitSystem unit_system) {
  return unit_system == UnitSystem::Metric ? MetricCandidates() : ImperialCandidates();
}

// Reference "fits" check, straight from the spec: double arithmetic, std::round (halfway away from zero).
bool Fits(const std::vector<float> &values, int factor, float epsilon) {
  const double f = factor;
  const double eps = epsilon;
  return std::all_of(values.begin(), values.end(), [&](float value) {
    const double v = value;
    const double q = std::round(v * f);
    return std::abs(q / f - v) <= eps;
  });
}

std::vector<float> RandomValues() {
  std::mt19937 rng{kSeed};
  std::uniform_real_distribution<float> dist(kLow, kHigh);
  std::vector<float> values(kSize);
  for (auto &value : values)
    value = dist(rng);
  return values;
}

using PropertyParam = std::tuple<UnitSystem, float>;

class PropertyTest : public testing::TestWithParam<PropertyParam> {};

std::string PropertyName(const testing::TestParamInfo<PropertyParam> &info) {
  const auto [unit_system, epsilon] = info.param;
  const std::string system = unit_system == UnitSystem::Metric ? "Metric" : "Imperial";
  const std::string eps = epsilon == 0.1f ? "Eps0_1" : epsilon == 0.01f ? "Eps0_01" : "Eps0_001";
  return system + "_" + eps;
}

} // namespace

TEST(PropertyCandidates, ReferenceListsHaveExpectedShape) {
  const auto metric = MetricCandidates();
  ASSERT_EQ(metric.size(), 10U);
  EXPECT_EQ(metric.front(), 1);
  EXPECT_EQ(metric.back(), 1'000'000'000);

  const auto imperial = ImperialCandidates();
  ASSERT_EQ(imperial.size(), 328U);
  EXPECT_EQ(imperial.front(), 1);
  EXPECT_EQ(imperial.back(), 2066242608);
  EXPECT_EQ(std::vector<int>(imperial.begin(), imperial.begin() + 20),
            (std::vector<int>{1, 2, 3, 4, 6, 8, 9, 12, 16, 18, 24, 27, 32, 36, 48, 54, 64, 72, 81, 96}));
  EXPECT_TRUE(std::is_sorted(imperial.begin(), imperial.end()));
  EXPECT_EQ(std::adjacent_find(imperial.begin(), imperial.end()), imperial.end());
}

TEST_P(PropertyTest, FactorIsSmallestFittingCandidate) {
  const auto [unit_system, epsilon] = GetParam();
  const auto values = RandomValues();
  const auto candidates = CandidatesFor(unit_system);

  const int factor = calculate_discretization_factor(values, epsilon, unit_system);

  const auto it = std::find(candidates.begin(), candidates.end(), factor);
  ASSERT_NE(it, candidates.end()) << "factor " << factor << " is not a candidate";
  EXPECT_TRUE(Fits(values, factor, epsilon)) << "factor " << factor << " does not fit";
  if (it != candidates.begin()) {
    EXPECT_FALSE(Fits(values, *(it - 1), epsilon)) << "previous candidate " << *(it - 1) << " also fits";
  }
}

TEST_P(PropertyTest, DiscretizeIsWithinEpsilon) {
  const auto [unit_system, epsilon] = GetParam();
  const auto values = RandomValues();

  const int factor = calculate_discretization_factor(values, epsilon, unit_system);
  const auto discretized = discretize(values, epsilon, unit_system);

  ASSERT_EQ(discretized.size(), values.size());
  const double f = factor;
  for (std::size_t i = 0; i < values.size(); ++i) {
    const double v = values[i];
    ASSERT_EQ(discretized[i], static_cast<int>(std::round(v * f))) << "index " << i;
    ASSERT_LE(std::abs(discretized[i] / f - v), static_cast<double>(epsilon)) << "index " << i;
  }
}

TEST_P(PropertyTest, RawMatchesVector) {
  const auto [unit_system, epsilon] = GetParam();
  const auto values = RandomValues();

  EXPECT_EQ(calculate_discretization_factor(values.data(), values.size(), epsilon, unit_system),
            calculate_discretization_factor(values, epsilon, unit_system));

  std::vector<int> out(values.size(), -123456);
  discretize(values.data(), values.size(), epsilon, out.data(), unit_system);
  EXPECT_EQ(out, discretize(values, epsilon, unit_system));
}

INSTANTIATE_TEST_SUITE_P(BothSystems, PropertyTest,
                         testing::Combine(testing::Values(UnitSystem::Metric, UnitSystem::Imperial),
                                          testing::Values(0.1f, 0.01f, 0.001f)),
                         PropertyName);
