#include "discretization/discretization.hpp"

#include <gtest/gtest.h>

#include <ostream>
#include <string>
#include <vector>

using discretization::calculate_discretization_factor;
using discretization::discretize;
using discretization::UnitSystem;

namespace {

struct Example {
  std::string name;
  std::vector<float> values;
  float epsilon;
  int factor;
  std::vector<int> discretized;
};

void PrintTo(const Example &example, std::ostream *os) { *os << example.name; }

std::string ExampleName(const testing::TestParamInfo<Example> &info) { return info.param.name; }

constexpr float kThird = 1.0f / 3.0f;
constexpr float kSevenTwelfths = 7.0f / 12.0f;

// Examples where both unit systems give the same result (factor 1, or empty input).
std::vector<Example> CommonExamples() {
  return {
      {"Integers", {3.0f, -7.0f, 0.0f}, 0.01f, 1, {3, -7, 0}},
      {"Nearest", {0.0f, 1.2f, 1.7f, -1.2f, -1.7f}, 0.5f, 1, {0, 1, 2, -1, -2}},
      {"HalfwayAwayFromZero", {0.5f, 1.5f, 2.5f, -0.5f, -2.5f}, 0.5f, 1, {1, 2, 3, -1, -3}},
      {"Empty", {}, 0.01f, 1, {}},
      {"SingleInteger", {42.0f}, 0.01f, 1, {42}},
  };
}

std::vector<Example> MetricExamples() {
  std::vector<Example> examples{
      {"Mixed", {0.1f, 2.5f, -3.7f}, 0.01f, 10, {1, 25, -37}},
      {"Fine1234", {1.234f}, 0.001f, 1000, {1234}},
      {"Coarse1234", {1.234f}, 0.005f, 100, {123}},
      {"Third", {kThird}, 1e-6f, 1000000, {333333}},
      {"Values", {0.4f, 1.6f, -2.5f}, 0.01f, 10, {4, 16, -25}},
      {"NegativeOnly", {-0.4f, -1.6f}, 0.01f, 10, {-4, -16}},
  };
  for (auto &example : CommonExamples())
    examples.push_back(example);
  return examples;
}

std::vector<Example> ImperialExamples() {
  std::vector<Example> examples{
      {"FiveSixteenths", {0.3125f}, 1e-6f, 16, {5}},
      {"NegativeFiveSixteenths", {-0.3125f}, 1e-6f, 16, {-5}},
      {"Third", {kThird}, 1e-6f, 3, {1}},
      {"SevenTwelfths", {kSevenTwelfths}, 1e-6f, 12, {7}},
      {"Quarters", {2.25f, 0.5f}, 1e-6f, 4, {9, 2}},
      {"ThirdAndQuarter", {kThird, 0.25f}, 1e-6f, 12, {4, 3}},
      {"Sixteenths", {0.5f, 0.25f, 0.3125f}, 1e-6f, 16, {8, 4, 5}},
      {"Mixed", {0.1f, 2.5f, -3.7f}, 0.01f, 48, {5, 120, -178}},
      {"Values", {0.4f, 1.6f, -2.5f}, 0.01f, 32, {13, 51, -80}},
  };
  for (auto &example : CommonExamples())
    examples.push_back(example);
  return examples;
}

int RawFactor(const Example &example, UnitSystem unit_system) {
  return calculate_discretization_factor(example.values.data(), example.values.size(), example.epsilon, unit_system);
}

std::vector<int> RawDiscretize(const Example &example, UnitSystem unit_system) {
  // Sentinel-filled so that unwritten entries are detected.
  std::vector<int> out(example.values.size(), -123456);
  discretize(example.values.data(), example.values.size(), example.epsilon, out.data(), unit_system);
  return out;
}

class MetricExamplesTest : public testing::TestWithParam<Example> {};

class ImperialExamplesTest : public testing::TestWithParam<Example> {};

} // namespace

TEST_P(MetricExamplesTest, FactorVector) {
  const auto &example = GetParam();
  EXPECT_EQ(calculate_discretization_factor(example.values, example.epsilon, UnitSystem::Metric), example.factor);
}

TEST_P(MetricExamplesTest, FactorRaw) {
  const auto &example = GetParam();
  EXPECT_EQ(RawFactor(example, UnitSystem::Metric), example.factor);
}

TEST_P(MetricExamplesTest, DiscretizeVector) {
  const auto &example = GetParam();
  EXPECT_EQ(discretize(example.values, example.epsilon, UnitSystem::Metric), example.discretized);
}

TEST_P(MetricExamplesTest, DiscretizeRaw) {
  const auto &example = GetParam();
  EXPECT_EQ(RawDiscretize(example, UnitSystem::Metric), example.discretized);
}

TEST_P(MetricExamplesTest, RawMatchesVector) {
  const auto &example = GetParam();
  EXPECT_EQ(RawFactor(example, UnitSystem::Metric),
            calculate_discretization_factor(example.values, example.epsilon, UnitSystem::Metric));
  EXPECT_EQ(RawDiscretize(example, UnitSystem::Metric),
            discretize(example.values, example.epsilon, UnitSystem::Metric));
}

TEST_P(MetricExamplesTest, DefaultMatchesExplicitMetric) {
  const auto &example = GetParam();
  EXPECT_EQ(calculate_discretization_factor(example.values, example.epsilon),
            calculate_discretization_factor(example.values, example.epsilon, UnitSystem::Metric));
  EXPECT_EQ(calculate_discretization_factor(example.values.data(), example.values.size(), example.epsilon),
            RawFactor(example, UnitSystem::Metric));
  EXPECT_EQ(discretize(example.values, example.epsilon),
            discretize(example.values, example.epsilon, UnitSystem::Metric));
  std::vector<int> out(example.values.size(), -123456);
  discretize(example.values.data(), example.values.size(), example.epsilon, out.data());
  EXPECT_EQ(out, RawDiscretize(example, UnitSystem::Metric));
}

INSTANTIATE_TEST_SUITE_P(UnitSystemExamples, MetricExamplesTest, testing::ValuesIn(MetricExamples()), ExampleName);

TEST_P(ImperialExamplesTest, FactorVector) {
  const auto &example = GetParam();
  EXPECT_EQ(calculate_discretization_factor(example.values, example.epsilon, UnitSystem::Imperial), example.factor);
}

TEST_P(ImperialExamplesTest, FactorRaw) {
  const auto &example = GetParam();
  EXPECT_EQ(RawFactor(example, UnitSystem::Imperial), example.factor);
}

TEST_P(ImperialExamplesTest, DiscretizeVector) {
  const auto &example = GetParam();
  EXPECT_EQ(discretize(example.values, example.epsilon, UnitSystem::Imperial), example.discretized);
}

TEST_P(ImperialExamplesTest, DiscretizeRaw) {
  const auto &example = GetParam();
  EXPECT_EQ(RawDiscretize(example, UnitSystem::Imperial), example.discretized);
}

TEST_P(ImperialExamplesTest, RawMatchesVector) {
  const auto &example = GetParam();
  EXPECT_EQ(RawFactor(example, UnitSystem::Imperial),
            calculate_discretization_factor(example.values, example.epsilon, UnitSystem::Imperial));
  EXPECT_EQ(RawDiscretize(example, UnitSystem::Imperial),
            discretize(example.values, example.epsilon, UnitSystem::Imperial));
}

INSTANTIATE_TEST_SUITE_P(UnitSystemExamples, ImperialExamplesTest, testing::ValuesIn(ImperialExamples()), ExampleName);

// The default argument must be UnitSystem::Metric. {0.4, 1.6, -2.5} discriminates: metric -> 10, imperial -> 32.
TEST(UnitSystemDefault, FactorDefaultsToMetric) {
  const std::vector<float> values{0.4f, 1.6f, -2.5f};
  EXPECT_EQ(calculate_discretization_factor(values, 0.01f), 10);
  EXPECT_EQ(calculate_discretization_factor(values.data(), values.size(), 0.01f), 10);
  EXPECT_EQ(calculate_discretization_factor(values, 0.01f),
            calculate_discretization_factor(values, 0.01f, UnitSystem::Metric));
  EXPECT_NE(calculate_discretization_factor(values, 0.01f),
            calculate_discretization_factor(values, 0.01f, UnitSystem::Imperial));
}

TEST(UnitSystemDefault, DiscretizeDefaultsToMetric) {
  const std::vector<float> values{0.4f, 1.6f, -2.5f};
  EXPECT_EQ(discretize(values, 0.01f), (std::vector<int>{4, 16, -25}));
  std::vector<int> out(values.size(), -123456);
  discretize(values.data(), values.size(), 0.01f, out.data());
  EXPECT_EQ(out, (std::vector<int>{4, 16, -25}));
  EXPECT_EQ(discretize(values, 0.01f), discretize(values, 0.01f, UnitSystem::Metric));
  EXPECT_NE(discretize(values, 0.01f), discretize(values, 0.01f, UnitSystem::Imperial));
}

// Empty raw input: the buffers may be null and `out` must not be touched.
TEST(UnitSystemEmpty, RawNullBuffers) {
  for (const auto unit_system : {UnitSystem::Metric, UnitSystem::Imperial}) {
    EXPECT_EQ(calculate_discretization_factor(nullptr, 0, 0.01f, unit_system), 1);
    EXPECT_NO_THROW(discretize(nullptr, 0, 0.01f, nullptr, unit_system));
  }
}
