#include "discretization/discretization.hpp"

#include <gtest/gtest.h>

#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

using discretization::calculate_discretization_factor;
using discretization::discretize;
using discretization::UnitSystem;

namespace {

constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();
constexpr float kInf = std::numeric_limits<float>::infinity();

class ErrorsTest : public testing::TestWithParam<UnitSystem> {};

std::string UnitSystemName(const testing::TestParamInfo<UnitSystem> &info) {
  return info.param == UnitSystem::Metric ? "Metric" : "Imperial";
}

// Checks that all four entry points (factor/discretize x vector/raw) throw `Exception`.
template <typename Exception>
void ExpectAllThrow(const std::vector<float> &values, float epsilon, UnitSystem unit_system) {
  EXPECT_THROW(calculate_discretization_factor(values, epsilon, unit_system), Exception);
  EXPECT_THROW(calculate_discretization_factor(values.data(), values.size(), epsilon, unit_system), Exception);
  EXPECT_THROW(discretize(values, epsilon, unit_system), Exception);
  std::vector<int> out(values.size());
  EXPECT_THROW(discretize(values.data(), values.size(), epsilon, out.data(), unit_system), Exception);
}

} // namespace

TEST_P(ErrorsTest, EpsilonZero) { ExpectAllThrow<std::invalid_argument>({0.4f, 1.6f, -2.5f}, 0.0f, GetParam()); }

TEST_P(ErrorsTest, EpsilonNegativeZero) {
  ExpectAllThrow<std::invalid_argument>({0.4f, 1.6f, -2.5f}, -0.0f, GetParam());
}

TEST_P(ErrorsTest, EpsilonNegative) { ExpectAllThrow<std::invalid_argument>({0.4f, 1.6f, -2.5f}, -0.01f, GetParam()); }

TEST_P(ErrorsTest, EpsilonNaN) { ExpectAllThrow<std::invalid_argument>({0.4f, 1.6f, -2.5f}, kNaN, GetParam()); }

// Epsilon is validated before the empty-input shortcut.
TEST_P(ErrorsTest, EpsilonInvalidOnEmptyInput) {
  ExpectAllThrow<std::invalid_argument>({}, 0.0f, GetParam());
  ExpectAllThrow<std::invalid_argument>({}, -1.0f, GetParam());
  ExpectAllThrow<std::invalid_argument>({}, kNaN, GetParam());
}

TEST_P(ErrorsTest, NaNValue) { ExpectAllThrow<std::invalid_argument>({0.4f, kNaN, -2.5f}, 0.01f, GetParam()); }

TEST_P(ErrorsTest, PosInfValue) { ExpectAllThrow<std::invalid_argument>({0.4f, 1.6f, kInf}, 0.01f, GetParam()); }

TEST_P(ErrorsTest, NegInfValue) { ExpectAllThrow<std::invalid_argument>({-kInf, 1.6f, -2.5f}, 0.01f, GetParam()); }

TEST_P(ErrorsTest, NonFiniteSingleValue) {
  ExpectAllThrow<std::invalid_argument>({kNaN}, 0.01f, GetParam());
  ExpectAllThrow<std::invalid_argument>({kInf}, 0.01f, GetParam());
  ExpectAllThrow<std::invalid_argument>({-kInf}, 0.01f, GetParam());
}

// round(3e9 * 1) already overflows int32, so the search stops at the first candidate.
TEST_P(ErrorsTest, Int32Overflow) { ExpectAllThrow<std::out_of_range>({3e9f}, 0.5f, GetParam()); }

TEST_P(ErrorsTest, Int32OverflowNegative) { ExpectAllThrow<std::out_of_range>({1.0f, -3e9f}, 0.5f, GetParam()); }

INSTANTIATE_TEST_SUITE_P(BothSystems, ErrorsTest, testing::Values(UnitSystem::Metric, UnitSystem::Imperial),
                         UnitSystemName);

// 0.1f is not within 1e-12 of any k / 10^n for n <= 9.
TEST(Errors, NoCandidateFitsMetric) { ExpectAllThrow<std::out_of_range>({0.1f}, 1e-12f, UnitSystem::Metric); }

TEST(Errors, NoCandidateFitsMetricDefault) {
  const std::vector<float> values{0.1f};
  EXPECT_THROW(calculate_discretization_factor(values, 1e-12f), std::out_of_range);
  EXPECT_THROW(calculate_discretization_factor(values.data(), values.size(), 1e-12f), std::out_of_range);
  EXPECT_THROW(discretize(values, 1e-12f), std::out_of_range);
  std::vector<int> out(values.size());
  EXPECT_THROW(discretize(values.data(), values.size(), 1e-12f, out.data()), std::out_of_range);
}
