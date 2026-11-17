import numpy as np


def _imperial_candidates() -> list:
    int32_max = np.iinfo(np.int32).max
    return sorted(
        2**a * 3**b for a in range(32) for b in range(21) if 2**a * 3**b <= int32_max
    )


class Constants:
    """
    Constants used in unit testing
    """

    EPSILON = 0.01

    VALUES = [0.4, 1.6, -2.5]
    DISCRETIZED = [4, 16, -25]
    DISCRETIZED_IMPERIAL = [13, 51, -80]

    BENCHMARK_SIZE = 1_000_000

    EMPTY = np.array([], dtype=np.float32)

    # Factor of VALUES at EPSILON for each unit system.
    FACTOR = 10
    FACTOR_IMPERIAL = 32

    # Candidate lists, in ascending order.
    METRIC_CANDIDATES = [10**k for k in range(10)]
    IMPERIAL_CANDIDATES = _imperial_candidates()
    IMPERIAL_CANDIDATES_COUNT = 328
    IMPERIAL_CANDIDATES_FIRST_20 = [
        1, 2, 3, 4, 6, 8, 9, 12, 16, 18, 24, 27, 32, 36, 48, 54, 64, 72, 81, 96
    ]  # fmt: skip
    IMPERIAL_CANDIDATES_LAST = 2_066_242_608

    # Examples: (id, values, epsilon, factor, discretized).
    # Both unit systems give the same result for these.
    COMMON_EXAMPLES = [
        ("integers", [3.0, -7.0, 0.0], 0.01, 1, [3, -7, 0]),
        ("nearest", [0.0, 1.2, 1.7, -1.2, -1.7], 0.5, 1, [0, 1, 2, -1, -2]),
        ("halfway", [0.5, 1.5, 2.5, -0.5, -2.5], 0.5, 1, [1, 2, 3, -1, -3]),
        ("empty", [], 0.01, 1, []),
        ("single-integer", [42.0], 0.01, 1, [42]),
    ]
    METRIC_EXAMPLES = [
        ("mixed", [0.1, 2.5, -3.7], 0.01, 10, [1, 25, -37]),
        ("1234-fine", [1.234], 0.001, 1000, [1234]),
        ("1234-coarse", [1.234], 0.005, 100, [123]),
        ("third", [1 / 3], 1e-6, 1_000_000, [333333]),
        ("values", [0.4, 1.6, -2.5], 0.01, 10, [4, 16, -25]),
        ("negative-only", [-0.4, -1.6], 0.01, 10, [-4, -16]),
        *COMMON_EXAMPLES,
    ]
    IMPERIAL_EXAMPLES = [
        ("5/16", [0.3125], 1e-6, 16, [5]),
        ("neg-5/16", [-0.3125], 1e-6, 16, [-5]),
        ("third", [1 / 3], 1e-6, 3, [1]),
        ("7/12", [7 / 12], 1e-6, 12, [7]),
        ("quarters", [2.25, 0.5], 1e-6, 4, [9, 2]),
        ("third-quarter", [1 / 3, 0.25], 1e-6, 12, [4, 3]),
        ("sixteenths", [0.5, 0.25, 0.3125], 1e-6, 16, [8, 4, 5]),
        ("mixed", [0.1, 2.5, -3.7], 0.01, 48, [5, 120, -178]),
        ("values", [0.4, 1.6, -2.5], 0.01, 32, [13, 51, -80]),
        *COMMON_EXAMPLES,
    ]

    # Accepted spellings of unit_system (strings are case-insensitive).
    METRIC_SPELLINGS = ["metric", "METRIC", "Metric"]
    IMPERIAL_SPELLINGS = ["imperial", "IMPERIAL", "Imperial"]
    INVALID_UNIT_SYSTEMS = ["cubits", "", " metric", "metric ", 0, 1, None]

    # Error cases.
    INVALID_EPSILONS = [0.0, -0.0, -0.01, float("nan")]
    NON_FINITE_VALUES = [
        [0.4, float("nan"), -2.5],
        [0.4, 1.6, float("inf")],
        [float("-inf"), 1.6, -2.5],
    ]
    NO_FIT_VALUES = [0.1]
    NO_FIT_EPSILON = 1e-12
    OVERFLOW_VALUES = [3e9]
    OVERFLOW_EPSILON = 0.5

    # Property test.
    PROPERTY_SEED = 42
    PROPERTY_SIZE = 10_000
    PROPERTY_LOW = -1000.0
    PROPERTY_HIGH = 1000.0
    PROPERTY_EPSILONS = [0.1, 0.01, 0.001]
