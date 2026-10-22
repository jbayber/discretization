#include "discretization/discretization.hpp"

#include <gtest/gtest.h>

#include <vector>

using discretization::calculate_discretization_factor;
using discretization::discretize;

TEST(CalculateDiscretizationFactor, VectorReturnsOne) {
  EXPECT_EQ(calculate_discretization_factor(std::vector<float>{0.1f, 2.5f, -3.7f}, 0.01f), 1);
}

TEST(CalculateDiscretizationFactor, EmptyVectorReturnsOne) {
  EXPECT_EQ(calculate_discretization_factor(std::vector<float>{}, 0.01f), 1);
}

TEST(CalculateDiscretizationFactor, ArrayMatchesVector) {
  const float values[] = {0.1f, 2.5f, -3.7f};
  EXPECT_EQ(calculate_discretization_factor(values, 3, 0.01f),
            calculate_discretization_factor(std::vector<float>(values, values + 3), 0.01f));
}

TEST(Discretize, VectorRoundsToNearest) {
  const std::vector<float> values{0.0f, 1.2f, 1.7f, -1.2f, -1.7f};
  EXPECT_EQ(discretize(values, 0.01f), (std::vector<int>{0, 1, 2, -1, -2}));
}

TEST(Discretize, HalfwayRoundsAwayFromZero) {
  const std::vector<float> values{0.5f, 1.5f, 2.5f, -0.5f, -2.5f};
  EXPECT_EQ(discretize(values, 0.01f), (std::vector<int>{1, 2, 3, -1, -3}));
}

TEST(Discretize, EmptyVector) { EXPECT_TRUE(discretize(std::vector<float>{}, 0.01f).empty()); }

TEST(Discretize, ArrayMatchesVector) {
  const float values[] = {0.4f, 1.6f, -2.5f, 10.49f};
  int out[4] = {};
  discretize(values, 4, 0.01f, out);
  EXPECT_EQ(std::vector<int>(out, out + 4), discretize(std::vector<float>(values, values + 4), 0.01f));
}
