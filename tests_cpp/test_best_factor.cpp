#include "discretization/discretization.hpp"
#include "discretization_internal.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <bit>
#include <cmath>
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

namespace {

constexpr int kDefaultBudget = std::numeric_limits<int>::max();
constexpr float kThird = 1.0f / 3.0f;
constexpr float kSevenTwelfths = 7.0f / 12.0f;
constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();
constexpr float kInf = std::numeric_limits<float>::infinity();

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

FactorResult Raw(const std::vector<float> &values, std::optional<UnitSystem> unit_system,
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

// Straightforward serial search over all candidates: one pass per candidate, values summed in index order.
FactorResult Reference(const std::vector<float> &values, std::optional<UnitSystem> unit_system, int max_factor) {
  const auto metric = MetricCandidates();
  if (values.empty())
    return {1, 0.0, UnitSystem::Metric};
  double max_abs = 0.0;
  for (float v : values)
    max_abs = std::max(max_abs, std::abs(static_cast<double>(v)));
  bool found = false;
  FactorResult best{};
  for (int f : CandidatesFor(unit_system)) {
    if (f > max_factor)
      break;
    const double fd = f;
    if (std::abs(std::round(max_abs * fd)) > static_cast<double>(std::numeric_limits<std::int32_t>::max()))
      break;
    double sum = 0.0;
    for (float v : values) {
      const double x = v;
      sum += std::abs(std::round(x * fd) / fd - x);
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

std::vector<float> RandomValues(std::uint32_t seed, std::size_t size, float low, float high) {
  // Portable on purpose, so the hardcoded expectations below hold on every platform:
  // - std::mt19937's raw output is fixed by the standard, but
  //   std::uniform_real_distribution is implementation-defined (libc++ and libstdc++
  //   produce different values).
  // - `low + (high - low) * u` may or may not be contracted into an FMA depending on
  //   compiler and target (Apple clang on arm64 fuses it, GCC on x86-64 doesn't), so
  //   the fused operation is spelled out with std::fma, which is exactly rounded.
  // The top 24 bits of each draw give an exact float u in [0, 1).
  std::mt19937 gen{seed};
  std::vector<float> values(size);
  for (auto &v : values)
    v = std::fma(high - low, static_cast<float>(gen() >> 8) * 0x1p-24f, low);
  return values;
}

// ---- Selection table ----

struct Selection {
  std::string name;
  std::vector<float> values;
  std::optional<UnitSystem> unit_system;
  int max_factor;
  int factor;
  double error;
  UnitSystem result_system;
};

void PrintTo(const Selection &selection, std::ostream *os) { *os << selection.name; }

std::string SelectionName(const testing::TestParamInfo<Selection> &info) { return info.param.name; }

std::vector<Selection> Selections() {
  return {
      {"EighthBudget100", {0.125f}, kMetric, 100, 100, 0.005, UnitSystem::Metric},
      {"EighthDefault", {0.125f}, kMetric, kDefaultBudget, 1000, 0.0, UnitSystem::Metric},
      {"HalfQuarterTieSmallest", {0.5f, 0.25f}, kMetric, kDefaultBudget, 100, 0.0, UnitSystem::Metric},
      // 10, 100 and 1000 tie exactly; the smallest wins.
      {"MixedExactTie", {0.1f, 2.5f, -3.7f}, kMetric, 1000, 10, 1.6391277252170156e-08, UnitSystem::Metric},
      {"BudgetOne", {2.5f}, kMetric, 1, 1, 0.5, UnitSystem::Metric},
      {"Integers", {3.0f, -7.0f, 0.0f}, kMetric, kDefaultBudget, 1, 0.0, UnitSystem::Metric},
      {"FiveSixteenths", {0.3125f}, kImperial, 1000, 16, 0.0, UnitSystem::Imperial},
      {"Third", {kThird}, kImperial, 100, 3, 9.934107481068821e-09, UnitSystem::Imperial},
      {"ThirdQuarter", {kThird, 0.25f}, kImperial, 100, 12, 4.967053740534411e-09, UnitSystem::Imperial},
      {"SevenTwelfths", {kSevenTwelfths}, kImperial, 100, 12, 1.9868214962137642e-08, UnitSystem::Imperial},
      {"ValuesImperial10", {0.4f, 1.6f, -2.5f}, kImperial, 10, 8, 0.01666666070620219, UnitSystem::Imperial},
      {"ValuesMetric10", {0.4f, 1.6f, -2.5f}, kMetric, 10, 10, 9.93410742555767e-09, UnitSystem::Metric},
      {"BothThird", {kThird}, kBoth, 100, 3, 9.934107481068821e-09, UnitSystem::Imperial},
      {"BothPointThree", {0.3f}, kBoth, 100, 10, 1.1920928966180355e-08, UnitSystem::Metric},
      {"BothFiveSixteenths", {0.3125f}, kBoth, kDefaultBudget, 16, 0.0, UnitSystem::Imperial},
      // float32 values are dyadic: with an unlimited budget the exact representation (2^24) wins.
      {"BothUnlimitedDyadic", {0.3f}, kBoth, kDefaultBudget, 16777216, 0.0, UnitSystem::Imperial},
      // 10 * 1e9 overflows int32, so only 1 is considered.
      {"OverflowCutoff", {1e9f}, kMetric, kDefaultBudget, 1, 0.0, UnitSystem::Metric},
  };
}

class BestFactorSelection : public testing::TestWithParam<Selection> {};

} // namespace

TEST_P(BestFactorSelection, Vector) {
  const auto &s = GetParam();
  ExpectResult(best_discretization_factor(s.values, s.unit_system, s.max_factor), s.factor, s.error, s.result_system);
}

TEST_P(BestFactorSelection, Raw) {
  const auto &s = GetParam();
  ExpectResult(Raw(s.values, s.unit_system, s.max_factor), s.factor, s.error, s.result_system);
}

TEST_P(BestFactorSelection, RawMatchesVector) {
  const auto &s = GetParam();
  ExpectIdentical(Raw(s.values, s.unit_system, s.max_factor),
                  best_discretization_factor(s.values, s.unit_system, s.max_factor));
}

TEST_P(BestFactorSelection, MatchesReference) {
  const auto &s = GetParam();
  ExpectIdentical(best_discretization_factor(s.values, s.unit_system, s.max_factor),
                  Reference(s.values, s.unit_system, s.max_factor));
}

TEST_P(BestFactorSelection, SingleThreadMatchesAuto) {
  const auto &s = GetParam();
  ExpectIdentical(best_discretization_factor(s.values, s.unit_system, s.max_factor, 1),
                  best_discretization_factor(s.values, s.unit_system, s.max_factor, 0));
}

INSTANTIATE_TEST_SUITE_P(BestFactor, BestFactorSelection, testing::ValuesIn(Selections()), SelectionName);

// ---- Defaults ----

TEST(BestFactorDefaults, DefaultArgumentsSearchBothWithUnlimitedBudget) {
  const std::vector<float> values{0.3125f};
  ExpectResult(best_discretization_factor(values), 16, 0.0, UnitSystem::Imperial);
  ExpectResult(best_discretization_factor(values.data(), values.size()), 16, 0.0, UnitSystem::Imperial);
  const std::vector<float> point_three{0.3f};
  ExpectIdentical(best_discretization_factor(point_three),
                  best_discretization_factor(point_three, std::nullopt, kDefaultBudget, 0));
  ExpectIdentical(best_discretization_factor(point_three.data(), point_three.size()),
                  best_discretization_factor(point_three.data(), point_three.size(), std::nullopt, kDefaultBudget, 0));
  EXPECT_EQ(best_discretization_factor(point_three).factor, 16777216);
}

// ---- Edge cases ----

TEST(BestFactorEdge, EmptyInput) {
  const std::vector<float> empty;
  for (const auto unit_system : kAllSystems) {
    SCOPED_TRACE(SystemName(unit_system));
    ExpectResult(best_discretization_factor(empty, unit_system), 1, 0.0, UnitSystem::Metric);
    ExpectResult(Raw(empty, unit_system), 1, 0.0, UnitSystem::Metric);
    ExpectResult(best_discretization_factor(nullptr, 0, unit_system), 1, 0.0, UnitSystem::Metric);
    ExpectResult(best_discretization_factor(nullptr, 0, unit_system, 1, 8), 1, 0.0, UnitSystem::Metric);
  }
}

TEST(BestFactorEdge, SingleNegativeValueMetric) {
  const std::vector<float> values{-0.125f};
  ExpectResult(best_discretization_factor(values, kMetric), 1000, 0.0, UnitSystem::Metric);
  ExpectResult(Raw(values, kMetric), 1000, 0.0, UnitSystem::Metric);
}

TEST(BestFactorEdge, SingleNegativeValueImperial) {
  const std::vector<float> values{-0.3125f};
  ExpectResult(best_discretization_factor(values, kImperial), 16, 0.0, UnitSystem::Imperial);
  ExpectResult(Raw(values, kImperial), 16, 0.0, UnitSystem::Imperial);
}

TEST(BestFactorEdge, AllZeros) {
  const std::vector<float> values(1000, 0.0f);
  for (const auto unit_system : kAllSystems) {
    SCOPED_TRACE(SystemName(unit_system));
    ExpectResult(best_discretization_factor(values, unit_system), 1, 0.0, UnitSystem::Metric);
    ExpectResult(Raw(values, unit_system), 1, 0.0, UnitSystem::Metric);
  }
}

// Factor 1 is in both lists and is always reported as Metric, also when Imperial was requested.
TEST(BestFactorEdge, FactorOneReportsMetricUnderImperial) {
  const std::vector<float> values{3.0f, -7.0f, 0.0f};
  for (const auto unit_system : kAllSystems) {
    SCOPED_TRACE(SystemName(unit_system));
    ExpectResult(best_discretization_factor(values, unit_system), 1, 0.0, UnitSystem::Metric);
    ExpectResult(Raw(values, unit_system), 1, 0.0, UnitSystem::Metric);
  }
}

TEST(BestFactorEdge, MaxFactorOne) {
  const std::vector<float> values{0.4f, 1.6f, -2.5f};
  for (const auto unit_system : kAllSystems) {
    SCOPED_TRACE(SystemName(unit_system));
    ExpectResult(best_discretization_factor(values, unit_system, 1), 1, 0.43333332737286884, UnitSystem::Metric);
    ExpectResult(Raw(values, unit_system, 1), 1, 0.43333332737286884, UnitSystem::Metric);
  }
}

// 50 isn't a metric candidate: the candidates are 1 and 10.
TEST(BestFactorEdge, MaxFactorNotACandidate) {
  const std::vector<float> values{0.125f};
  ExpectResult(best_discretization_factor(values, kMetric, 50), 10, 0.024999999999999994, UnitSystem::Metric);
  ExpectResult(Raw(values, kMetric, 50), 10, 0.024999999999999994, UnitSystem::Metric);
}

// max|v| = 1e9: Metric stops at 1, Imperial still has 2 (2e9 <= INT32_MAX).
TEST(BestFactorEdge, OverflowCutoffWithHalf) {
  const std::vector<float> values{1e9f, 0.5f};
  ExpectResult(best_discretization_factor(values, kMetric), 1, 0.25, UnitSystem::Metric);
  ExpectResult(Raw(values, kMetric), 1, 0.25, UnitSystem::Metric);
  ExpectResult(best_discretization_factor(values, kImperial), 2, 0.0, UnitSystem::Imperial);
  ExpectResult(Raw(values, kImperial), 2, 0.0, UnitSystem::Imperial);
  ExpectResult(best_discretization_factor(values, kBoth), 2, 0.0, UnitSystem::Imperial);
  ExpectResult(Raw(values, kBoth), 2, 0.0, UnitSystem::Imperial);
}

TEST(BestFactorEdge, OverflowCutoffImperialAndBoth) {
  const std::vector<float> values{1e9f};
  for (const auto unit_system : {kImperial, kBoth}) {
    SCOPED_TRACE(SystemName(unit_system));
    ExpectResult(best_discretization_factor(values, unit_system), 1, 0.0, UnitSystem::Metric);
    ExpectResult(Raw(values, unit_system), 1, 0.0, UnitSystem::Metric);
  }
}

// The largest float32 below 2^31 still fits at f = 1.
TEST(BestFactorEdge, LargestFloatBelowInt32Limit) {
  const std::vector<float> values{2147483520.0f, -2147483520.0f};
  for (const auto unit_system : kAllSystems) {
    SCOPED_TRACE(SystemName(unit_system));
    ExpectResult(best_discretization_factor(values, unit_system), 1, 0.0, UnitSystem::Metric);
    ExpectResult(Raw(values, unit_system), 1, 0.0, UnitSystem::Metric);
  }
}

// ---- Errors ----

TEST(BestFactorErrors, NonFiniteValues) {
  const std::vector<std::vector<float>> inputs{
      {0.4f, kNaN, -2.5f}, {0.4f, 1.6f, kInf}, {-kInf, 1.6f, -2.5f}, {kNaN}, {3e9f, kNaN}};
  for (const auto unit_system : kAllSystems) {
    for (const auto &values : inputs) {
      SCOPED_TRACE(SystemName(unit_system));
      EXPECT_THROW(best_discretization_factor(values, unit_system), std::invalid_argument);
      EXPECT_THROW(Raw(values, unit_system), std::invalid_argument);
      EXPECT_THROW(Raw(values, unit_system, 10, 1), std::invalid_argument);
    }
  }
}

TEST(BestFactorErrors, InvalidMaxFactor) {
  const std::vector<float> values{0.4f, 1.6f, -2.5f};
  const std::vector<float> empty;
  for (const int max_factor : {0, -1, std::numeric_limits<int>::min()}) {
    for (const auto unit_system : kAllSystems) {
      SCOPED_TRACE(SystemName(unit_system) + " max_factor=" + std::to_string(max_factor));
      EXPECT_THROW(best_discretization_factor(values, unit_system, max_factor), std::invalid_argument);
      EXPECT_THROW(Raw(values, unit_system, max_factor), std::invalid_argument);
      EXPECT_THROW(best_discretization_factor(empty, unit_system, max_factor), std::invalid_argument);
      EXPECT_THROW(best_discretization_factor(nullptr, 0, unit_system, max_factor), std::invalid_argument);
    }
  }
}

TEST(BestFactorErrors, NegativeNumThreads) {
  const std::vector<float> values{0.4f, 1.6f, -2.5f};
  const std::vector<float> empty;
  for (const int num_threads : {-1, -8, std::numeric_limits<int>::min()}) {
    for (const auto unit_system : kAllSystems) {
      SCOPED_TRACE(SystemName(unit_system) + " num_threads=" + std::to_string(num_threads));
      EXPECT_THROW(best_discretization_factor(values, unit_system, kDefaultBudget, num_threads), std::invalid_argument);
      EXPECT_THROW(Raw(values, unit_system, kDefaultBudget, num_threads), std::invalid_argument);
      EXPECT_THROW(best_discretization_factor(empty, unit_system, kDefaultBudget, num_threads), std::invalid_argument);
      EXPECT_THROW(best_discretization_factor(nullptr, 0, unit_system, kDefaultBudget, num_threads),
                   std::invalid_argument);
    }
  }
}

TEST(BestFactorErrors, InvalidUnitSystemEnum) {
  const std::vector<float> values{0.4f, 1.6f, -2.5f};
  const auto bogus = static_cast<UnitSystem>(42);
  EXPECT_THROW(best_discretization_factor(values, bogus), std::invalid_argument);
  EXPECT_THROW(Raw(values, bogus), std::invalid_argument);
}

// |round(v)| > INT32_MAX already at f = 1: no candidate is considered.
TEST(BestFactorErrors, OverflowAtFactorOne) {
  const std::vector<std::vector<float>> inputs{{3e9f}, {0.5f, -3e9f}, {2147483648.0f}, {-2147483648.0f}};
  for (const auto unit_system : kAllSystems) {
    for (const auto &values : inputs) {
      SCOPED_TRACE(SystemName(unit_system));
      EXPECT_THROW(best_discretization_factor(values, unit_system), std::out_of_range);
      EXPECT_THROW(Raw(values, unit_system), std::out_of_range);
      EXPECT_THROW(best_discretization_factor(values, unit_system, 1), std::out_of_range);
      EXPECT_THROW(Raw(values, unit_system, 1), std::out_of_range);
    }
  }
}

// Errors found by worker threads must reach the caller, not terminate the process.
TEST(BestFactorErrors, ErrorsFromLargeParallelInput) {
  auto values = RandomValues(7, 1'000'000, -1000.0f, 1000.0f);
  for (const int num_threads : {0, 3, 8}) {
    SCOPED_TRACE("num_threads=" + std::to_string(num_threads));
    auto with_nan = values;
    with_nan.back() = kNaN;
    EXPECT_THROW(best_discretization_factor(with_nan, kMetric, kDefaultBudget, num_threads), std::invalid_argument);
    auto with_inf = values;
    with_inf[with_inf.size() / 2] = -kInf;
    EXPECT_THROW(Raw(with_inf, kImperial, kDefaultBudget, num_threads), std::invalid_argument);
    auto with_overflow = values;
    with_overflow.back() = 3e9f;
    EXPECT_THROW(best_discretization_factor(with_overflow, kBoth, kDefaultBudget, num_threads), std::out_of_range);
  }
}

// ---- Threads (internal header) ----

static_assert(detail::kChunkSize >= 10'000, "property tests assume 10k values fit in one chunk");

namespace {

long long Chunks(std::size_t size) {
  return static_cast<long long>((size + detail::kChunkSize - 1) / detail::kChunkSize);
}

long long Resolve(std::size_t size, int num_threads, unsigned hw) {
  return static_cast<long long>(detail::ResolveThreadCount(size, num_threads, hw));
}

long long Spawned() { return static_cast<long long>(detail::SpawnedThreadCount()); }

} // namespace

TEST(BestFactorThreads, ResolveAutoBelowThresholdIsSerial) {
  const std::size_t below = static_cast<std::size_t>(detail::kAutoParallelThreshold) - 1;
  EXPECT_EQ(Resolve(below, 0, 8), 1);
  EXPECT_EQ(Resolve(below, 0, 64), 1);
  EXPECT_EQ(Resolve(1, 0, 8), 1);
  EXPECT_EQ(Resolve(0, 0, 8), 1);
}

TEST(BestFactorThreads, ResolveAutoAtThreshold) {
  const std::size_t at = static_cast<std::size_t>(detail::kAutoParallelThreshold);
  EXPECT_EQ(Resolve(at, 0, 8), std::min(8LL, Chunks(at)));
}

TEST(BestFactorThreads, ResolveAutoUsesHardwareConcurrency) {
  const std::size_t size = 100 * static_cast<std::size_t>(detail::kChunkSize);
  EXPECT_EQ(Resolve(size, 0, 8), 8);
  EXPECT_EQ(Resolve(size, 0, 3), 3);
}

TEST(BestFactorThreads, ResolveAutoWithUnknownHardwareIsSerial) {
  EXPECT_EQ(Resolve(1'000'000, 0, 0), 1);
  EXPECT_EQ(Resolve(100 * static_cast<std::size_t>(detail::kChunkSize), 0, 0), 1);
}

TEST(BestFactorThreads, ResolveExplicitIsCappedToChunks) {
  EXPECT_EQ(Resolve(1'000'000, 64, 8), std::min(64LL, Chunks(1'000'000)));
  EXPECT_EQ(Resolve(1'000'000, 3, 8), std::min(3LL, Chunks(1'000'000)));
  EXPECT_EQ(Resolve(1'000'000, 1, 8), 1);
  // Explicit counts ignore the auto threshold but are still capped to the number of chunks.
  EXPECT_EQ(Resolve(1000, 4, 8), 1);
  EXPECT_EQ(Resolve(0, 4, 8), 1);
  EXPECT_EQ(Resolve(2 * static_cast<std::size_t>(detail::kChunkSize), 5, 1), 2);
}

TEST(BestFactorThreads, NoSpawnBelowThreshold) {
  const auto values = RandomValues(11, static_cast<std::size_t>(detail::kAutoParallelThreshold) - 1, -1000.0f, 1000.0f);
  const auto before = Spawned();
  best_discretization_factor(values, kMetric);
  Raw(values, kBoth);
  EXPECT_EQ(Spawned() - before, 0);
}

TEST(BestFactorThreads, NoSpawnWithOneThread) {
  const auto values = RandomValues(11, 1'000'000, -1000.0f, 1000.0f);
  const auto before = Spawned();
  best_discretization_factor(values, kMetric, kDefaultBudget, 1);
  EXPECT_EQ(Spawned() - before, 0);
}

TEST(BestFactorThreads, SpawnsAboveThreshold) {
  const auto values = RandomValues(11, 1'000'000, -1000.0f, 1000.0f);
  const auto before = Spawned();
  best_discretization_factor(values, kMetric, kDefaultBudget, 3);
  // The calling thread also works, so 3 threads means 2 spawned per parallel pass (pre-pass and main pass).
  const auto spawned = Spawned() - before;
  const auto threads = std::min(3LL, Chunks(values.size()));
  EXPECT_GT(spawned, 0);
  EXPECT_EQ(spawned % (threads - 1), 0);
  EXPECT_LE(spawned, 2 * (threads - 1));
}

// ---- Determinism ----

namespace {

class BestFactorDeterminism : public testing::TestWithParam<std::optional<UnitSystem>> {};

std::string DeterminismName(const testing::TestParamInfo<std::optional<UnitSystem>> &info) {
  return SystemName(info.param);
}

const std::vector<float> &DeterminismValues() {
  static const std::vector<float> values = RandomValues(12345, 1'000'000, -1000.0f, 1000.0f);
  return values;
}

} // namespace

TEST_P(BestFactorDeterminism, BitIdenticalAcrossThreadCounts) {
  const auto unit_system = GetParam();
  const auto &values = DeterminismValues();
  const auto serial = best_discretization_factor(values, unit_system, kDefaultBudget, 1);
  for (const int num_threads : {0, 2, 3, 8, 64}) {
    SCOPED_TRACE("num_threads=" + std::to_string(num_threads));
    ExpectIdentical(best_discretization_factor(values, unit_system, kDefaultBudget, num_threads), serial);
  }
  ExpectIdentical(best_discretization_factor(values.data(), values.size(), unit_system, kDefaultBudget, 3), serial);
}

INSTANTIATE_TEST_SUITE_P(BestFactor, BestFactorDeterminism, testing::ValuesIn(kAllSystems), DeterminismName);

// ---- Property: brute-force serial reference ----

namespace {

struct PropertyCase {
  std::optional<UnitSystem> unit_system;
  int max_factor;
  int factor;
  UnitSystem result_system;
};

void PrintTo(const PropertyCase &c, std::ostream *os) { *os << SystemName(c.unit_system) << " " << c.max_factor; }

std::string PropertyName(const testing::TestParamInfo<PropertyCase> &info) {
  return SystemName(info.param.unit_system) + "Budget" + std::to_string(info.param.max_factor);
}

class BestFactorProperty : public testing::TestWithParam<PropertyCase> {};

const std::vector<float> &PropertyValues() {
  static const std::vector<float> values = RandomValues(42, 10'000, -1000.0f, 1000.0f);
  return values;
}

} // namespace

TEST_P(BestFactorProperty, MatchesBruteForceReference) {
  const auto &c = GetParam();
  const auto &values = PropertyValues();
  const auto reference = Reference(values, c.unit_system, c.max_factor);
  EXPECT_EQ(reference.factor, c.factor);
  EXPECT_EQ(reference.unit_system, c.result_system);
  // 10k values fit in one chunk, so the error must match the serial reference exactly.
  ExpectIdentical(best_discretization_factor(values, c.unit_system, c.max_factor), reference);
  ExpectIdentical(best_discretization_factor(values.data(), values.size(), c.unit_system, c.max_factor), reference);
  ExpectIdentical(best_discretization_factor(values, c.unit_system, c.max_factor, 4), reference);
}

INSTANTIATE_TEST_SUITE_P(BestFactor, BestFactorProperty,
                         testing::Values(PropertyCase{kMetric, 10, 10, UnitSystem::Metric},
                                         PropertyCase{kMetric, 1000, 1000, UnitSystem::Metric},
                                         PropertyCase{kMetric, 1'000'000, 1'000'000, UnitSystem::Metric},
                                         PropertyCase{kImperial, 10, 9, UnitSystem::Imperial},
                                         PropertyCase{kImperial, 1000, 972, UnitSystem::Imperial},
                                         PropertyCase{kImperial, 1'000'000, 786432, UnitSystem::Imperial},
                                         PropertyCase{kBoth, 10, 10, UnitSystem::Metric},
                                         PropertyCase{kBoth, 1000, 1000, UnitSystem::Metric},
                                         PropertyCase{kBoth, 1'000'000, 786432, UnitSystem::Imperial}),
                         PropertyName);
