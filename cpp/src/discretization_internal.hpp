#pragma once

// Internal helpers of `best_discretization_factor`, exposed for the C++ tests only.
// Not part of the public API: not installed and not included by the public header.

#include <cstddef>
#include <cstdint>

namespace discretization::detail {

// Number of values per chunk. The input is split into fixed-size chunks, independent of the
// thread count, and the per-chunk partial sums are reduced in chunk order, so results are
// bit-identical for any number of threads.
inline constexpr std::size_t kChunkSize = 65'536;

// With `num_threads == 0` (auto), inputs with fewer values run on the calling thread.
inline constexpr std::size_t kAutoParallelThreshold = 100'000;

// Number of threads (including the calling thread) used for `size` values:
// `num_threads > 0` is used as given; `0` means 1 below `kAutoParallelThreshold`, else `hw`
// (1 if `hw == 0`). The result is capped to the number of chunks and is at least 1.
std::size_t ResolveThreadCount(std::size_t size, int num_threads, unsigned hw);

// Total number of worker threads spawned so far by the library (for tests).
std::uint64_t SpawnedThreadCount();

} // namespace discretization::detail
