// Tests of the `double` versions of `best_discretization_factor` (specs/best-factor-float64).
//
// The helpers are duplicated from test_best_factor.cpp on purpose: that file must not be edited.

#include "discretization/discretization.hpp"
#include "discretization_internal.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <ostream>
#include <random>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

using discretization::best_discretization_factor;
using discretization::FactorResult;
using discretization::UnitSystem;

namespace detail = discretization::detail;

// ---- Overload resolution (compile-time) ----
//
// Adding the double versions must not make existing float calls ambiguous or reroute them:
// - `nullptr` can't deduce the double template, so it reaches the float raw-buffer version;
// - `float *` / `const float *` / `std::vector<float>` keep resolving to the float versions;
// - `double *` / `const double *` / `std::vector<double>` resolve to the double versions;
// - other element types are not accepted.

namespace {

template <class P>
concept RawCallable = requires(P p) { best_discretization_factor(p, std::size_t{0}); };

template <class P>
concept TemplateCallable = requires(P p) { best_discretization_factor<>(p, std::size_t{0}); };

template <class V>
concept VectorCallable = requires(const V &v) { best_discretization_factor(v); };

static_assert(RawCallable<std::nullptr_t>);
static_assert(!TemplateCallable<std::nullptr_t>);
static_assert(TemplateCallable<const double *> && TemplateCallable<double *>);
static_assert(!TemplateCallable<const float *> && !TemplateCallable<float *>);
static_assert(RawCallable<const float *> && RawCallable<float *>);
static_assert(RawCallable<const double *> && RawCallable<double *>);
static_assert(!RawCallable<const int *> && !RawCallable<const long double *>);
static_assert(VectorCallable<std::vector<float>> && VectorCallable<std::vector<double>>);
static_assert(!VectorCallable<std::vector<int>>);

constexpr int kDefaultBudget = std::numeric_limits<int>::max();
constexpr double kThird = 1.0 / 3.0;
constexpr double kSevenTwelfths = 7.0 / 12.0;
constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf = std::numeric_limits<double>::infinity();
constexpr double kDenormMin = std::numeric_limits<double>::denorm_min();
constexpr double kDoubleMax = std::numeric_limits<double>::max();
// float16(0.1) widened to double.
constexpr double kFloat16Tenth = 0.0999755859375;

constexpr std::optional<UnitSystem> kMetric = UnitSystem::Metric;
constexpr std::optional<UnitSystem> kImperial = UnitSystem::Imperial;
constexpr std::optional<UnitSystem> kBoth = std::nullopt;

const std::vector<std::optional<UnitSystem>> kAllSystems{kMetric, kImperial, kBoth};

std::string SystemName(std::optional<UnitSystem> unit_system) {
  if (!unit_system)
    return "Both";
  return *unit_system == UnitSystem::Metric ? "Metric" : "Imperial";
}

// Errors are compared exactly when the expected value is 0, else to 1e-12 relative.
void ExpectError(double actual, double expected) {
  if (expected == 0.0) {
    EXPECT_EQ(actual, 0.0);
  } else {
    EXPECT_NEAR(actual, expected, 1e-12 * std::abs(expected));
  }
}

void ExpectResult(const FactorResult &actual, int factor, double error, UnitSystem unit_system) {
  EXPECT_EQ(actual.factor, factor);
  ExpectError(actual.mean_abs_error, error);
  EXPECT_EQ(actual.unit_system, unit_system);
}

void ExpectIdentical(const FactorResult &actual, const FactorResult &expected) {
  EXPECT_EQ(actual.factor, expected.factor);
  EXPECT_EQ(std::bit_cast<std::uint64_t>(actual.mean_abs_error), std::bit_cast<std::uint64_t>(expected.mean_abs_error))
      << actual.mean_abs_error << " vs " << expected.mean_abs_error;
  EXPECT_EQ(actual.unit_system, expected.unit_system);
}

FactorResult Raw(const std::vector<double> &values, std::optional<UnitSystem> unit_system,
                 int max_factor = kDefaultBudget, int num_threads = 0) {
  return best_discretization_factor(values.data(), values.size(), unit_system, max_factor, num_threads);
}

FactorResult RawFloat(const std::vector<float> &values, std::optional<UnitSystem> unit_system,
                      int max_factor = kDefaultBudget, int num_threads = 0) {
  return best_discretization_factor(values.data(), values.size(), unit_system, max_factor, num_threads);
}

// ---- Reference candidate lists and brute-force search, independent of the library ----

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

std::vector<int> BothCandidates() {
  std::set<int> merged;
  for (int f : MetricCandidates())
    merged.insert(f);
  for (int f : ImperialCandidates())
    merged.insert(f);
  return {merged.begin(), merged.end()};
}

std::vector<int> CandidatesFor(std::optional<UnitSystem> unit_system) {
  if (!unit_system)
    return BothCandidates();
  return *unit_system == UnitSystem::Metric ? MetricCandidates() : ImperialCandidates();
}

// Straightforward serial search in double over all candidates: one pass per candidate, values summed in index order.
FactorResult Reference(const std::vector<double> &values, std::optional<UnitSystem> unit_system, int max_factor) {
  const auto metric = MetricCandidates();
  if (values.empty())
    return {1, 0.0, UnitSystem::Metric};
  double max_abs = 0.0;
  for (double v : values)
    max_abs = std::max(max_abs, std::abs(v));
  bool found = false;
  FactorResult best{};
  for (int f : CandidatesFor(unit_system)) {
    if (f > max_factor)
      break;
    const double fd = f;
    if (std::abs(std::round(max_abs * fd)) > static_cast<double>(std::numeric_limits<std::int32_t>::max()))
      break;
    double sum = 0.0;
    for (double x : values) {
      const double q = std::round(x * fd);
      sum += std::abs(q / fd - x);
    }
    const double error = sum / static_cast<double>(values.size());
    if (!found || error < best.mean_abs_error) {
      found = true;
      const bool is_metric = std::find(metric.begin(), metric.end(), f) != metric.end();
      best = {f, error, is_metric ? UnitSystem::Metric : UnitSystem::Imperial};
    }
  }
  if (!found)
    throw std::out_of_range("reference: no candidate");
  return best;
}

// Portable on purpose, so the hardcoded expectations below hold on every platform:
// - std::mt19937_64's raw output is fixed by the standard, but std::uniform_real_distribution is
//   implementation-defined (libc++ and libstdc++ produce different values).
// - `low + (high - low) * u` may or may not be contracted into an FMA depending on compiler and
//   target, so the fused operation is spelled out with std::fma, which is exactly rounded.
// The top 53 bits of each draw give an exact double u in [0, 1).
std::vector<double> RandomDoubles(std::uint64_t seed, std::size_t size, double low, double high) {
  std::mt19937_64 gen{seed};
  std::vector<double> values(size);
  for (auto &v : values)
    v = std::fma(high - low, static_cast<double>(gen() >> 11) * 0x1p-53, low);
  return values;
}

// Same as RandomValues in test_best_factor.cpp: exact float32 data, portable.
std::vector<float> RandomFloats(std::uint32_t seed, std::size_t size, float low, float high) {
  std::mt19937 gen{seed};
  std::vector<float> values(size);
  for (auto &v : values)
    v = std::fma(high - low, static_cast<float>(gen() >> 8) * 0x1p-24f, low);
  return values;
}

// ---- Selection table ----

struct Selection {
  std::string name;
  std::vector<double> values;
  std::optional<UnitSystem> unit_system;
  int max_factor;
  int factor;
  double error;
  UnitSystem result_system;
};

void PrintTo(const Selection &selection, std::ostream *os) { *os << selection.name; }

std::string SelectionName(const testing::TestParamInfo<Selection> &info) { return info.param.name; }

std::vector<Selection> Selections() {
  constexpr auto kM = UnitSystem::Metric;
  constexpr auto kI = UnitSystem::Imperial;
  return {
      // ---- Float64 selection (spec acceptance criteria) ----
      // float32 gives 10 with 1.64e-8; in double, 10, 100 and 1000 are exact and tie at 0.
      {"MixedBudget1000", {0.1, 2.5, -3.7}, kMetric, 1000, 10, 0.0, kM},
      // float32 gives 1e8.
      {"MixedDefault", {0.1, 2.5, -3.7}, kMetric, kDefaultBudget, 10, 0.0, kM},
      {"TenthMetric", {0.1}, kMetric, kDefaultBudget, 10, 0.0, kM},
      // float32 gives 134217728, Imperial.
      {"TenthBoth", {0.1}, kBoth, kDefaultBudget, 10, 0.0, kM},
      // float32 gives 16777216, Imperial.
      {"PointThreeBoth", {0.3}, kBoth, kDefaultBudget, 10, 0.0, kM},
      {"Decimal1234", {1.234}, kMetric, kDefaultBudget, 1000, 0.0, kM},
      {"CentimetreInches", {12.7, 2.54, 30.48}, kBoth, kDefaultBudget, 100, 0.0, kM},
      {"BinaryFractions", {0.375, 1.5625, 2.75}, kBoth, kDefaultBudget, 16, 0.0, kI},
      {"ThirdImperial100", {kThird}, kImperial, 100, 3, 0.0, kI},
      {"ThirdBoth", {kThird}, kBoth, kDefaultBudget, 3, 0.0, kI},
      {"ThirdQuarterImperial100", {kThird, 0.25}, kImperial, 100, 12, 0.0, kI},
      {"SevenTwelfthsBoth", {kSevenTwelfths}, kBoth, kDefaultBudget, 12, 0.0, kI},
      {"ValuesImperial10", {0.4, 1.6, -2.5}, kImperial, 10, 8, 0.016666666666666646, kI},
      {"ValuesMetric10", {0.4, 1.6, -2.5}, kMetric, 10, 10, 0.0, kM},
      {"ValuesBoth10", {0.4, 1.6, -2.5}, kBoth, 10, 10, 0.0, kM},
      {"MaxFactorOneBoth", {0.4, 1.6, -2.5}, kBoth, 1, 1, 0.4333333333333333, kM},
      // ---- Same results as float32 where the input is exactly representable in both ----
      {"EighthBudget100", {0.125}, kMetric, 100, 100, 0.005, kM},
      {"EighthDefault", {0.125}, kMetric, kDefaultBudget, 1000, 0.0, kM},
      // 50 isn't a metric candidate: the candidates are 1 and 10.
      {"EighthBudget50", {0.125}, kMetric, 50, 10, 0.024999999999999994, kM},
      {"HalfQuarterTieSmallest", {0.5, 0.25}, kMetric, kDefaultBudget, 100, 0.0, kM},
      {"BudgetOne", {2.5}, kMetric, 1, 1, 0.5, kM},
      {"IntegersMetric", {3.0, -7.0, 0.0}, kMetric, kDefaultBudget, 1, 0.0, kM},
      {"IntegersImperial", {3.0, -7.0, 0.0}, kImperial, kDefaultBudget, 1, 0.0, kM},
      {"IntegersBoth", {3.0, -7.0, 0.0}, kBoth, kDefaultBudget, 1, 0.0, kM},
      {"FiveSixteenths", {0.3125}, kImperial, 1000, 16, 0.0, kI},
      // 10 * 1e9 overflows int32, so only 1 is considered.
      {"BillionMetric", {1e9}, kMetric, kDefaultBudget, 1, 0.0, kM},
      {"BillionImperial", {1e9}, kImperial, kDefaultBudget, 1, 0.0, kM},
      {"BillionBoth", {1e9}, kBoth, kDefaultBudget, 1, 0.0, kM},
      // ---- Edge rows ----
      {"NegativeEighth", {-0.125}, kMetric, kDefaultBudget, 1000, 0.0, kM},
      {"NegativeFiveSixteenths", {-0.3125}, kImperial, kDefaultBudget, 16, 0.0, kI},
      // Every candidate rounds 5e-324 to 0, so all errors tie at denorm_min and 1 wins.
      {"SubnormalMetric", {kDenormMin}, kMetric, kDefaultBudget, 1, kDenormMin, kM},
      {"SubnormalImperial", {kDenormMin}, kImperial, kDefaultBudget, 1, kDenormMin, kM},
      {"SubnormalBoth", {kDenormMin}, kBoth, kDefaultBudget, 1, kDenormMin, kM},
      // 2^24 + 1 isn't a float32, but it is an exact double.
      {"AboveTwoTo24", {16777217.0}, kBoth, kDefaultBudget, 1, 0.0, kM},
      {"AboveTwoTo24Metric", {16777217.0}, kMetric, kDefaultBudget, 1, 0.0, kM},
      {"AboveTwoTo24Imperial", {16777217.0}, kImperial, kDefaultBudget, 1, 0.0, kM},
      // float32 rounds it to 16777218 (factor 1).
      {"HalfAboveTwoTo24", {16777217.5}, kBoth, kDefaultBudget, 2, 0.0, kI},
      {"Float16TenthBoth", {kFloat16Tenth}, kBoth, kDefaultBudget, 8192, 0.0, kI},
      {"Float16TenthImperial", {kFloat16Tenth}, kImperial, kDefaultBudget, 8192, 0.0, kI},
      {"Float16TenthMetric", {kFloat16Tenth}, kMetric, kDefaultBudget, 1'000'000'000, 6.250000517127319e-11, kM},
      {"Int32Limit", {2147483647.0, -2147483647.0}, kBoth, kDefaultBudget, 1, 0.0, kM},
      {"NearInt32Limit", {2147483647.4}, kBoth, kDefaultBudget, 1, 0.40000009536743164, kM},
      // max|v| = 1e9: Metric stops at 1, Imperial still has 2 (2e9 <= INT32_MAX).
      {"OverflowHalfMetric", {1e9, 0.5}, kMetric, kDefaultBudget, 1, 0.25, kM},
      {"OverflowHalfImperial", {1e9, 0.5}, kImperial, kDefaultBudget, 2, 0.0, kI},
      {"OverflowHalfBoth", {1e9, 0.5}, kBoth, kDefaultBudget, 2, 0.0, kI},
  };
}

class BestFactorFloat64Selection : public testing::TestWithParam<Selection> {};

} // namespace

TEST_P(BestFactorFloat64Selection, Vector) {
  const auto &s = GetParam();
  ExpectResult(best_discretization_factor(s.values, s.unit_system, s.max_factor), s.factor, s.error, s.result_system);
}

TEST_P(BestFactorFloat64Selection, Raw) {
  const auto &s = GetParam();
  ExpectResult(Raw(s.values, s.unit_system, s.max_factor), s.factor, s.error, s.result_system);
}

TEST_P(BestFactorFloat64Selection, RawMatchesVector) {
  const auto &s = GetParam();
  ExpectIdentical(Raw(s.values, s.unit_system, s.max_factor),
                  best_discretization_factor(s.values, s.unit_system, s.max_factor));
}

TEST_P(BestFactorFloat64Selection, MatchesReference) {
  const auto &s = GetParam();
  ExpectIdentical(best_discretization_factor(s.values, s.unit_system, s.max_factor),
                  Reference(s.values, s.unit_system, s.max_factor));
}

TEST_P(BestFactorFloat64Selection, SingleThreadMatchesAuto) {
  const auto &s = GetParam();
  ExpectIdentical(best_discretization_factor(s.values, s.unit_system, s.max_factor, 1),
                  best_discretization_factor(s.values, s.unit_system, s.max_factor, 0));
}

INSTANTIATE_TEST_SUITE_P(BestFactorFloat64, BestFactorFloat64Selection, testing::ValuesIn(Selections()), SelectionName);

// ---- Overloads (runtime) ----

TEST(BestFactorFloat64Overloads, AddressOfEachOverload) {
  using FloatVector = FactorResult (*)(const std::vector<float> &, std::optional<UnitSystem>, int, int);
  using FloatRaw = FactorResult (*)(const float *, std::size_t, std::optional<UnitSystem>, int, int);
  using DoubleVector = FactorResult (*)(const std::vector<double> &, std::optional<UnitSystem>, int, int);
  using DoubleRaw = FactorResult (*)(const double *, std::size_t, std::optional<UnitSystem>, int, int);
  const FloatVector float_vector = &best_discretization_factor;
  const FloatRaw float_raw = &best_discretization_factor;
  const DoubleVector double_vector = &best_discretization_factor;
  const DoubleRaw double_raw = &best_discretization_factor;

  const std::vector<float> floats{0.3f};
  const std::vector<double> doubles{0.3};
  // float32 0.3 is dyadic: the exact representation 2^24 wins. In double, 10 is exact.
  ExpectResult(float_vector(floats, kBoth, kDefaultBudget, 0), 16777216, 0.0, UnitSystem::Imperial);
  ExpectResult(float_raw(floats.data(), floats.size(), kBoth, kDefaultBudget, 0), 16777216, 0.0, UnitSystem::Imperial);
  ExpectResult(double_vector(doubles, kBoth, kDefaultBudget, 0), 10, 0.0, UnitSystem::Metric);
  ExpectResult(double_raw(doubles.data(), doubles.size(), kBoth, kDefaultBudget, 0), 10, 0.0, UnitSystem::Metric);
}

TEST(BestFactorFloat64Overloads, NullptrStillCallsFloatVersion) {
  // Compiles only if the call is unambiguous (see the static_asserts above).
  ExpectResult(best_discretization_factor(nullptr, 0), 1, 0.0, UnitSystem::Metric);
  ExpectResult(best_discretization_factor(nullptr, 0, kImperial, 1, 8), 1, 0.0, UnitSystem::Metric);
}

TEST(BestFactorFloat64Overloads, DefaultArguments) {
  const std::vector<double> values{0.3};
  ExpectResult(best_discretization_factor(values), 10, 0.0, UnitSystem::Metric);
  ExpectResult(best_discretization_factor(values.data(), values.size()), 10, 0.0, UnitSystem::Metric);
  ExpectIdentical(best_discretization_factor(values),
                  best_discretization_factor(values, std::nullopt, kDefaultBudget, 0));
  ExpectIdentical(best_discretization_factor(values.data(), values.size()),
                  best_discretization_factor(values.data(), values.size(), std::nullopt, kDefaultBudget, 0));
}

// ---- Edge cases ----

TEST(BestFactorFloat64Edge, EmptyInput) {
  const std::vector<double> empty;
  const double *null_values = nullptr;
  for (const auto unit_system : kAllSystems) {
    SCOPED_TRACE(SystemName(unit_system));
    ExpectResult(best_discretization_factor(empty, unit_system), 1, 0.0, UnitSystem::Metric);
    ExpectResult(Raw(empty, unit_system), 1, 0.0, UnitSystem::Metric);
    ExpectResult(best_discretization_factor(null_values, 0, unit_system), 1, 0.0, UnitSystem::Metric);
    ExpectResult(best_discretization_factor(null_values, 0, unit_system, 1, 8), 1, 0.0, UnitSystem::Metric);
  }
}

TEST(BestFactorFloat64Edge, SingleValue) {
  const std::vector<double> values{0.1};
  ExpectResult(best_discretization_factor(values), 10, 0.0, UnitSystem::Metric);
  ExpectResult(Raw(values, kBoth), 10, 0.0, UnitSystem::Metric);
  ExpectResult(best_discretization_factor(values, kMetric), 10, 0.0, UnitSystem::Metric);
  ExpectResult(Raw(values, kMetric), 10, 0.0, UnitSystem::Metric);
}

TEST(BestFactorFloat64Edge, AllZeros) {
  const std::vector<double> values(1000, 0.0);
  for (const auto unit_system : kAllSystems) {
    SCOPED_TRACE(SystemName(unit_system));
    ExpectResult(best_discretization_factor(values, unit_system), 1, 0.0, UnitSystem::Metric);
    ExpectResult(Raw(values, unit_system), 1, 0.0, UnitSystem::Metric);
  }
}

// Factor 1 is in both lists and is always reported as Metric, also when Imperial was requested.
TEST(BestFactorFloat64Edge, FactorOneReportsMetricUnderImperial) {
  const std::vector<double> values{3.0, -7.0, 0.0};
  for (const auto unit_system : kAllSystems) {
    SCOPED_TRACE(SystemName(unit_system));
    ExpectResult(best_discretization_factor(values, unit_system), 1, 0.0, UnitSystem::Metric);
    ExpectResult(Raw(values, unit_system), 1, 0.0, UnitSystem::Metric);
  }
}

// Subnormal input is not flushed to zero: the error is exactly denorm_min for every candidate,
// so the smallest factor wins.
TEST(BestFactorFloat64Edge, Subnormal) {
  for (const double value : {kDenormMin, -kDenormMin}) {
    const std::vector<double> values{value};
    for (const auto unit_system : kAllSystems) {
      SCOPED_TRACE(SystemName(unit_system) + (value < 0 ? " negative" : " positive"));
      const auto vector_result = best_discretization_factor(values, unit_system);
      EXPECT_EQ(vector_result.factor, 1);
      EXPECT_EQ(vector_result.mean_abs_error, kDenormMin);
      EXPECT_EQ(vector_result.unit_system, UnitSystem::Metric);
      ExpectIdentical(Raw(values, unit_system), vector_result);
    }
  }
}

// ---- Errors ----

TEST(BestFactorFloat64Errors, NonFiniteValues) {
  const std::vector<std::vector<double>> inputs{
      {0.4, kNaN, -2.5}, {0.4, 1.6, kInf}, {-kInf, 1.6, -2.5}, {kNaN}, {kInf}, {-kInf}, {3e9, kNaN}};
  for (const auto unit_system : kAllSystems) {
    for (const auto &values : inputs) {
      SCOPED_TRACE(SystemName(unit_system));
      EXPECT_THROW(best_discretization_factor(values, unit_system), std::invalid_argument);
      EXPECT_THROW(Raw(values, unit_system), std::invalid_argument);
      EXPECT_THROW(Raw(values, unit_system, 10, 1), std::invalid_argument);
    }
  }
}

// |round(v)| > INT32_MAX already at f = 1: no candidate is considered.
TEST(BestFactorFloat64Errors, OverflowAtFactorOne) {
  const std::vector<std::vector<double>> inputs{
      {2147483647.5}, {-2147483648.0}, {3e9}, {0.5, -3e9}, {2147483648.0}, {1e300}, {1e39}, {kDoubleMax}};
  for (const auto unit_system : kAllSystems) {
    for (const auto &values : inputs) {
      SCOPED_TRACE(SystemName(unit_system) + " first=" + std::to_string(values.front()));
      EXPECT_THROW(best_discretization_factor(values, unit_system), std::out_of_range);
      EXPECT_THROW(Raw(values, unit_system), std::out_of_range);
      EXPECT_THROW(best_discretization_factor(values, unit_system, 1), std::out_of_range);
      EXPECT_THROW(Raw(values, unit_system, 1), std::out_of_range);
    }
  }
}

TEST(BestFactorFloat64Errors, InvalidMaxFactor) {
  const std::vector<double> values{0.4, 1.6, -2.5};
  const std::vector<double> empty;
  const double *null_values = nullptr;
  for (const int max_factor : {0, -1, std::numeric_limits<int>::min()}) {
    for (const auto unit_system : kAllSystems) {
      SCOPED_TRACE(SystemName(unit_system) + " max_factor=" + std::to_string(max_factor));
      EXPECT_THROW(best_discretization_factor(values, unit_system, max_factor), std::invalid_argument);
      EXPECT_THROW(Raw(values, unit_system, max_factor), std::invalid_argument);
      EXPECT_THROW(best_discretization_factor(empty, unit_system, max_factor), std::invalid_argument);
      EXPECT_THROW(best_discretization_factor(null_values, 0, unit_system, max_factor), std::invalid_argument);
    }
  }
}

TEST(BestFactorFloat64Errors, NegativeNumThreads) {
  const std::vector<double> values{0.4, 1.6, -2.5};
  const std::vector<double> empty;
  const double *null_values = nullptr;
  for (const int num_threads : {-1, -8, std::numeric_limits<int>::min()}) {
    for (const auto unit_system : kAllSystems) {
      SCOPED_TRACE(SystemName(unit_system) + " num_threads=" + std::to_string(num_threads));
      EXPECT_THROW(best_discretization_factor(values, unit_system, kDefaultBudget, num_threads), std::invalid_argument);
      EXPECT_THROW(Raw(values, unit_system, kDefaultBudget, num_threads), std::invalid_argument);
      EXPECT_THROW(best_discretization_factor(empty, unit_system, kDefaultBudget, num_threads), std::invalid_argument);
      EXPECT_THROW(best_discretization_factor(null_values, 0, unit_system, kDefaultBudget, num_threads),
                   std::invalid_argument);
    }
  }
}

TEST(BestFactorFloat64Errors, InvalidUnitSystemEnum) {
  const std::vector<double> values{0.4, 1.6, -2.5};
  const auto bogus = static_cast<UnitSystem>(42);
  EXPECT_THROW(best_discretization_factor(values, bogus), std::invalid_argument);
  EXPECT_THROW(Raw(values, bogus), std::invalid_argument);
}

// Errors found by worker threads must reach the caller, not terminate the process.
TEST(BestFactorFloat64Errors, ErrorsFromLargeParallelInput) {
  const auto values = RandomDoubles(7, 1'000'000, -1000.0, 1000.0);
  for (const int num_threads : {0, 3, 8}) {
    SCOPED_TRACE("num_threads=" + std::to_string(num_threads));
    auto with_nan = values;
    with_nan.back() = kNaN;
    EXPECT_THROW(best_discretization_factor(with_nan, kMetric, kDefaultBudget, num_threads), std::invalid_argument);
    auto with_inf = values;
    with_inf[with_inf.size() / 2] = -kInf;
    EXPECT_THROW(Raw(with_inf, kImperial, kDefaultBudget, num_threads), std::invalid_argument);
    auto with_overflow = values;
    with_overflow.back() = 3e9;
    EXPECT_THROW(best_discretization_factor(with_overflow, kBoth, kDefaultBudget, num_threads), std::out_of_range);
  }
}

// ---- Float32 vs float64 ----

// Where float32 can't represent the input exactly, the float version chases large factors.
TEST(BestFactorFloat64VsFloat32, DifferentWhereFloat32IsInexact) {
  const std::vector<float> mixed_f{0.1f, 2.5f, -3.7f};
  const std::vector<double> mixed_d{0.1, 2.5, -3.7};
  ExpectResult(best_discretization_factor(mixed_f, kMetric), 100'000'000, 1.2688000557610952e-09, UnitSystem::Metric);
  ExpectResult(RawFloat(mixed_f, kMetric), 100'000'000, 1.2688000557610952e-09, UnitSystem::Metric);
  ExpectResult(best_discretization_factor(mixed_d, kMetric), 10, 0.0, UnitSystem::Metric);
  ExpectResult(Raw(mixed_d, kMetric), 10, 0.0, UnitSystem::Metric);

  ExpectResult(best_discretization_factor(mixed_f, kMetric, 1000), 10, 1.6391277252170156e-08, UnitSystem::Metric);
  ExpectResult(best_discretization_factor(mixed_d, kMetric, 1000), 10, 0.0, UnitSystem::Metric);

  const std::vector<float> tenth_f{0.1f};
  const std::vector<double> tenth_d{0.1};
  ExpectResult(best_discretization_factor(tenth_f, kBoth), 134217728, 0.0, UnitSystem::Imperial);
  ExpectResult(best_discretization_factor(tenth_d, kBoth), 10, 0.0, UnitSystem::Metric);

  const std::vector<float> point_three_f{0.3f};
  const std::vector<double> point_three_d{0.3};
  ExpectResult(best_discretization_factor(point_three_f, kBoth), 16777216, 0.0, UnitSystem::Imperial);
  ExpectResult(best_discretization_factor(point_three_d, kBoth), 10, 0.0, UnitSystem::Metric);
}

TEST(BestFactorFloat64VsFloat32, IdenticalWhereExactlyRepresentable) {
  struct Case {
    std::vector<double> values;
    std::optional<UnitSystem> unit_system;
    int max_factor;
  };

  const std::vector<Case> cases{
      {{0.125}, kMetric, 100},
      {{0.125}, kMetric, kDefaultBudget},
      {{0.5, 0.25}, kMetric, kDefaultBudget},
      {{2.5}, kMetric, 1},
      {{3.0, -7.0, 0.0}, kMetric, kDefaultBudget},
      {{3.0, -7.0, 0.0}, kImperial, kDefaultBudget},
      {{3.0, -7.0, 0.0}, kBoth, kDefaultBudget},
      {{0.3125}, kImperial, 1000},
      {{1e9}, kMetric, kDefaultBudget},
  };
  for (const auto &c : cases) {
    SCOPED_TRACE(SystemName(c.unit_system) + " first=" + std::to_string(c.values.front()) +
                 " max_factor=" + std::to_string(c.max_factor));
    std::vector<float> floats(c.values.begin(), c.values.end());
    for (std::size_t i = 0; i < floats.size(); ++i)
      ASSERT_EQ(static_cast<double>(floats[i]), c.values[i]) << "not exactly representable";
    const auto float_result = best_discretization_factor(floats, c.unit_system, c.max_factor);
    ExpectIdentical(best_discretization_factor(c.values, c.unit_system, c.max_factor), float_result);
    ExpectIdentical(Raw(c.values, c.unit_system, c.max_factor), float_result);
  }
}

// The double version of widened float32 data is bit-identical to the float version: both compute in double.
TEST(BestFactorFloat64VsFloat32, Float64OfFloat32DataMatchesFloatVersion) {
  const auto floats = RandomFloats(42, 10'000, -1000.0f, 1000.0f);
  const std::vector<double> doubles(floats.begin(), floats.end());
  for (const auto unit_system : kAllSystems) {
    for (const int max_factor : {10, 1000, 1'000'000}) {
      SCOPED_TRACE(SystemName(unit_system) + " max_factor=" + std::to_string(max_factor));
      const auto float_result = best_discretization_factor(floats, unit_system, max_factor);
      ExpectIdentical(best_discretization_factor(doubles, unit_system, max_factor), float_result);
      ExpectIdentical(Raw(doubles, unit_system, max_factor), float_result);
    }
  }
}

// ---- Threads (internal header) ----

static_assert(detail::kChunkSize >= 10'000, "property tests assume 10k values fit in one chunk");

namespace {

long long Spawned() { return static_cast<long long>(detail::SpawnedThreadCount()); }

long long Chunks(std::size_t size) {
  return static_cast<long long>((size + detail::kChunkSize - 1) / detail::kChunkSize);
}

} // namespace

TEST(BestFactorFloat64Threads, NoSpawnBelowThreshold) {
  const auto values = RandomDoubles(11, static_cast<std::size_t>(detail::kAutoParallelThreshold) - 1, -1000.0, 1000.0);
  const auto before = Spawned();
  best_discretization_factor(values, kMetric);
  Raw(values, kBoth);
  EXPECT_EQ(Spawned() - before, 0);
}

TEST(BestFactorFloat64Threads, NoSpawnWithOneThread) {
  const auto values = RandomDoubles(11, 1'000'000, -1000.0, 1000.0);
  const auto before = Spawned();
  best_discretization_factor(values, kMetric, kDefaultBudget, 1);
  EXPECT_EQ(Spawned() - before, 0);
}

TEST(BestFactorFloat64Threads, SpawnsAboveThreshold) {
  const auto values = RandomDoubles(11, 1'000'000, -1000.0, 1000.0);
  const auto before = Spawned();
  best_discretization_factor(values, kMetric, kDefaultBudget, 3);
  // The calling thread also works, so 3 threads means 2 spawned per parallel pass (pre-pass and main pass).
  const auto spawned = Spawned() - before;
  const auto threads = std::min(3LL, Chunks(values.size()));
  EXPECT_GT(spawned, 0);
  EXPECT_LE(spawned, 2 * (threads - 1));
}

// ---- Determinism ----

namespace {

class BestFactorFloat64Determinism : public testing::TestWithParam<std::optional<UnitSystem>> {};

std::string DeterminismName(const testing::TestParamInfo<std::optional<UnitSystem>> &info) {
  return SystemName(info.param);
}

const std::vector<double> &DeterminismValues() {
  static const std::vector<double> values = RandomDoubles(12345, 1'000'000, -1000.0, 1000.0);
  return values;
}

} // namespace

TEST_P(BestFactorFloat64Determinism, BitIdenticalAcrossThreadCounts) {
  const auto unit_system = GetParam();
  const auto &values = DeterminismValues();
  const auto serial = best_discretization_factor(values, unit_system, kDefaultBudget, 1);
  for (const int num_threads : {0, 2, 3, 8, 64}) {
    SCOPED_TRACE("num_threads=" + std::to_string(num_threads));
    ExpectIdentical(best_discretization_factor(values, unit_system, kDefaultBudget, num_threads), serial);
  }
  ExpectIdentical(Raw(values, unit_system, kDefaultBudget, 3), serial);
}

INSTANTIATE_TEST_SUITE_P(BestFactorFloat64, BestFactorFloat64Determinism, testing::ValuesIn(kAllSystems),
                         DeterminismName);

// ---- Property: brute-force serial reference in double ----

namespace {

struct PropertyCase {
  std::optional<UnitSystem> unit_system;
  int max_factor;
  // Expected factor and list, or std::nullopt to check against the reference only (gap to the
  // runner-up below 1%, too close to hardcode across platforms).
  std::optional<int> factor;
  std::optional<UnitSystem> result_system;
};

void PrintTo(const PropertyCase &c, std::ostream *os) { *os << SystemName(c.unit_system) << " " << c.max_factor; }

std::string PropertyName(const testing::TestParamInfo<PropertyCase> &info) {
  return SystemName(info.param.unit_system) + "Budget" + std::to_string(info.param.max_factor);
}

class BestFactorFloat64Property : public testing::TestWithParam<PropertyCase> {};

const std::vector<double> &PropertyValues() {
  static const std::vector<double> values = RandomDoubles(42, 10'000, -1000.0, 1000.0);
  return values;
}

} // namespace

TEST_P(BestFactorFloat64Property, MatchesBruteForceReference) {
  const auto &c = GetParam();
  const auto &values = PropertyValues();
  const auto reference = Reference(values, c.unit_system, c.max_factor);
  if (c.factor) {
    EXPECT_EQ(reference.factor, *c.factor);
    EXPECT_EQ(reference.unit_system, *c.result_system);
  }
  // 10k values fit in one chunk, so the error must match the serial reference exactly.
  ExpectIdentical(best_discretization_factor(values, c.unit_system, c.max_factor), reference);
  ExpectIdentical(Raw(values, c.unit_system, c.max_factor), reference);
  ExpectIdentical(best_discretization_factor(values, c.unit_system, c.max_factor, 4), reference);
}

INSTANTIATE_TEST_SUITE_P(BestFactorFloat64, BestFactorFloat64Property,
                         testing::Values(PropertyCase{kMetric, 10, 10, UnitSystem::Metric},
                                         PropertyCase{kMetric, 1000, 1000, UnitSystem::Metric},
                                         PropertyCase{kMetric, 1'000'000, 1'000'000, UnitSystem::Metric},
                                         PropertyCase{kImperial, 10, 9, UnitSystem::Imperial},
                                         PropertyCase{kImperial, 1000, 972, UnitSystem::Imperial},
                                         PropertyCase{kImperial, 1'000'000, 995328, UnitSystem::Imperial},
                                         PropertyCase{kBoth, 10, 10, UnitSystem::Metric},
                                         PropertyCase{kBoth, 1000, 1000, UnitSystem::Metric},
                                         PropertyCase{kBoth, 1'000'000, std::nullopt, std::nullopt}),
                         PropertyName);
