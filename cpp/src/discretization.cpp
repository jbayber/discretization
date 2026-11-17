#include "discretization/discretization.hpp"
#include "discretization_internal.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <exception>
#include <limits>
#include <mutex>
#include <span>
#include <stdexcept>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

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

// Sorted union of both lists without duplicates (only 1 is in both).
constexpr std::size_t CountBothCandidates() {
  std::size_t count = kImperialCandidates.size();
  for (const int f : kMetricCandidates) {
    bool shared = false;
    for (const int g : kImperialCandidates)
      shared = shared || f == g;
    if (!shared)
      ++count;
  }
  return count;
}

constexpr std::size_t kBothCount = CountBothCandidates();

constexpr std::array<int, kBothCount> BuildBothCandidates() {
  std::array<int, kBothCount> out{};
  std::size_t i = 0;
  std::size_t j = 0;
  std::size_t n = 0;
  while (i < kMetricCandidates.size() || j < kImperialCandidates.size()) {
    if (j == kImperialCandidates.size() ||
        (i < kMetricCandidates.size() && kMetricCandidates[i] < kImperialCandidates[j])) {
      out[n++] = kMetricCandidates[i++];
    } else if (i == kMetricCandidates.size() || kImperialCandidates[j] < kMetricCandidates[i]) {
      out[n++] = kImperialCandidates[j++];
    } else {
      out[n++] = kMetricCandidates[i++];
      ++j;
    }
  }
  return out;
}

constexpr std::array<int, kBothCount> kBothCandidates = BuildBothCandidates();

static_assert(kBothCount == 337);
static_assert(kBothCandidates.front() == 1);
static_assert(kBothCandidates.back() == 2066242608);
static_assert(IsStrictlyAscending(kBothCandidates));

std::span<const int> CandidatesFor(UnitSystem unit_system) {
  switch (unit_system) {
  case UnitSystem::Metric:
    return kMetricCandidates;
  case UnitSystem::Imperial:
    return kImperialCandidates;
  }
  throw std::invalid_argument("unknown unit system");
}

std::span<const int> CandidatesFor(std::optional<UnitSystem> unit_system) {
  if (!unit_system)
    return kBothCandidates;
  return CandidatesFor(*unit_system);
}

bool IsMetricCandidate(int factor) {
  return std::find(kMetricCandidates.begin(), kMetricCandidates.end(), factor) != kMetricCandidates.end();
}

std::atomic<std::uint64_t> g_spawned_threads{0};

// Runs `body(chunk)` for every chunk in [0, num_chunks) on `threads` threads: the calling thread
// plus `threads - 1` spawned ones, which take chunks from a shared atomic counter. The caller
// writes each chunk's result to its own slot, so the outcome does not depend on scheduling.
// Exceptions thrown by `body` are captured and the first one is rethrown after all threads join.
// If a thread cannot be created, the threads that exist (at least the calling one) do the work.
template <typename Body> void ForEachChunk(std::size_t num_chunks, std::size_t threads, const Body &body) {
  if (threads <= 1) {
    for (std::size_t c = 0; c < num_chunks; ++c)
      body(c);
    return;
  }

  std::atomic<std::size_t> next{0};
  std::exception_ptr error;
  std::mutex error_mutex;
  auto worker = [&]() noexcept {
    try {
      for (std::size_t c = next.fetch_add(1, std::memory_order_relaxed); c < num_chunks;
           c = next.fetch_add(1, std::memory_order_relaxed)) {
        body(c);
      }
    } catch (...) {
      const std::lock_guard lock(error_mutex);
      if (!error)
        error = std::current_exception();
      next.store(num_chunks, std::memory_order_relaxed); // stop handing out chunks
    }
  };

  struct Joiner {
    std::vector<std::thread> threads;

    ~Joiner() {
      for (auto &t : threads)
        t.join();
    }
  } joiner;

  joiner.threads.reserve(threads - 1);
  for (std::size_t t = 1; t < threads; ++t) {
    try {
      joiner.threads.emplace_back(worker);
    } catch (const std::system_error &) {
      break; // could not create more threads: continue with the ones we have
    }
    g_spawned_threads.fetch_add(1, std::memory_order_relaxed);
  }
  worker();
  for (auto &t : joiner.threads)
    t.join();
  joiner.threads.clear();
  if (error)
    std::rethrow_exception(error);
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

namespace detail {

std::size_t ResolveThreadCount(std::size_t size, int num_threads, unsigned hw) {
  const std::size_t chunks = (size + kChunkSize - 1) / kChunkSize;
  std::size_t requested = 1;
  if (num_threads > 0) {
    requested = static_cast<std::size_t>(num_threads);
  } else if (size >= kAutoParallelThreshold) {
    requested = std::max<std::size_t>(hw, 1);
  }
  return std::max<std::size_t>(1, std::min(requested, chunks));
}

std::uint64_t SpawnedThreadCount() { return g_spawned_threads.load(std::memory_order_relaxed); }

} // namespace detail

FactorResult best_discretization_factor(const float *values, std::size_t size, std::optional<UnitSystem> unit_system,
                                        int max_factor, int num_threads) {
  using detail::kChunkSize;

  // Validate the arguments before anything else (including the empty-input shortcut).
  if (max_factor < 1)
    throw std::invalid_argument("max_factor must be >= 1");
  if (num_threads < 0)
    throw std::invalid_argument("num_threads must be >= 0 (0 = auto)");
  const std::span<const int> all_candidates = CandidatesFor(unit_system);
  if (size == 0)
    return {1, 0.0, UnitSystem::Metric};

  const std::size_t num_chunks = (size + kChunkSize - 1) / kChunkSize;
  const std::size_t threads = detail::ResolveThreadCount(size, num_threads, std::thread::hardware_concurrency());
  auto chunk_bounds = [&](std::size_t c) {
    const std::size_t begin = c * kChunkSize;
    return std::pair{begin, std::min(begin + kChunkSize, size)};
  };

  // Pre-pass: max |v| per chunk (for the overflow cutoff) and a non-finite flag.
  std::vector<float> chunk_max(num_chunks, 0.0f);
  std::vector<unsigned char> chunk_bad(num_chunks, 0);
  ForEachChunk(num_chunks, threads, [&](std::size_t c) {
    const auto [begin, end] = chunk_bounds(c);
    float m = 0.0f;
    bool bad = false;
    for (std::size_t i = begin; i < end; ++i) {
      const float a = std::abs(values[i]);
      bad |= !(a <= std::numeric_limits<float>::max()); // NaN or inf
      m = a > m ? a : m;
    }
    chunk_max[c] = m;
    chunk_bad[c] = bad ? 1 : 0;
  });
  if (std::find(chunk_bad.begin(), chunk_bad.end(), 1) != chunk_bad.end())
    throw std::invalid_argument("values must be finite (no NaN or inf)");
  const double max_abs = static_cast<double>(*std::max_element(chunk_max.begin(), chunk_max.end()));

  // Cutoff: max |round(v * f)| = round(max|v| * f) grows with f, so the usable candidates are a
  // prefix of the sorted list.
  std::vector<double> factors;
  for (const int candidate : all_candidates) {
    const double f = static_cast<double>(candidate);
    if (candidate > max_factor || std::round(max_abs * f) > static_cast<double>(kInt32Max))
      break;
    factors.push_back(f);
  }
  if (factors.empty())
    throw std::out_of_range("discretized value overflows int32 already with factor 1");
  const std::size_t k_count = factors.size();

  // Main pass: one read of each chunk accumulates the errors of all candidates. Sums run in index
  // order within a chunk; true division q / f matches the definition of err(f) exactly.
  std::vector<double> partial(num_chunks * k_count, 0.0);
  ForEachChunk(num_chunks, threads, [&](std::size_t c) {
    const auto [begin, end] = chunk_bounds(c);
    // Local accumulators: keeps the sums in registers/stack and avoids false sharing on `partial`.
    std::array<double, kBothCount> acc{};
    const double *f = factors.data();
    for (std::size_t i = begin; i < end; ++i) {
      const double x = static_cast<double>(values[i]);
      for (std::size_t k = 0; k < k_count; ++k)
        acc[k] += std::abs(std::round(x * f[k]) / f[k] - x);
    }
    std::copy_n(acc.begin(), k_count, partial.begin() + static_cast<std::ptrdiff_t>(c * k_count));
  });

  // Reduction in chunk order; the first strict minimum wins, so ties go to the smallest factor.
  const double n = static_cast<double>(size);
  std::size_t best = 0;
  double best_error = 0.0;
  for (std::size_t k = 0; k < k_count; ++k) {
    double total = 0.0;
    for (std::size_t c = 0; c < num_chunks; ++c)
      total += partial[c * k_count + k];
    const double error = total / n;
    if (k == 0 || error < best_error) {
      best = k;
      best_error = error;
    }
  }
  const int factor = static_cast<int>(factors[best]);
  return {factor, best_error, IsMetricCandidate(factor) ? UnitSystem::Metric : UnitSystem::Imperial};
}

FactorResult best_discretization_factor(const std::vector<float> &values, std::optional<UnitSystem> unit_system,
                                        int max_factor, int num_threads) {
  return best_discretization_factor(values.data(), values.size(), unit_system, max_factor, num_threads);
}

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
