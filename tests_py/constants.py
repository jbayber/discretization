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

    # ---- best_discretization_factor ----

    INT32_MAX = 2**31 - 1
    BOTH_CANDIDATES = sorted(set(METRIC_CANDIDATES) | set(IMPERIAL_CANDIDATES))
    BOTH_CANDIDATES_COUNT = 337

    # Selection examples: (id, values, unit_system, max_factor, factor, error,
    # result unit system). unit_system None searches both lists; max_factor
    # None uses the default budget. Errors are checked to 1e-12 relative
    # (exactly when 0).
    BEST_FACTOR_EXAMPLES = [
        ("eighth-budget-100", [0.125], "metric", 100, 100, 0.005, "metric"),
        ("eighth-default", [0.125], "metric", None, 1000, 0.0, "metric"),
        ("half-quarter-tie", [0.5, 0.25], "metric", None, 100, 0.0, "metric"),
        (
            "mixed-exact-tie",
            [0.1, 2.5, -3.7],
            "metric",
            1000,
            10,
            1.6391277252170156e-08,
            "metric",
        ),
        ("budget-one", [2.5], "metric", 1, 1, 0.5, "metric"),
        ("integers", [3.0, -7.0, 0.0], "metric", None, 1, 0.0, "metric"),
        ("5/16", [0.3125], "imperial", 1000, 16, 0.0, "imperial"),
        ("third", [1 / 3], "imperial", 100, 3, 9.934107481068821e-09, "imperial"),
        (
            "third-quarter",
            [1 / 3, 0.25],
            "imperial",
            100,
            12,
            4.967053740534411e-09,
            "imperial",
        ),
        ("7/12", [7 / 12], "imperial", 100, 12, 1.9868214962137642e-08, "imperial"),
        (
            "values-imperial-10",
            [0.4, 1.6, -2.5],
            "imperial",
            10,
            8,
            0.01666666070620219,
            "imperial",
        ),
        (
            "values-metric-10",
            [0.4, 1.6, -2.5],
            "metric",
            10,
            10,
            9.93410742555767e-09,
            "metric",
        ),
        ("both-third", [1 / 3], None, 100, 3, 9.934107481068821e-09, "imperial"),
        ("both-0.3", [0.3], None, 100, 10, 1.1920928966180355e-08, "metric"),
        ("both-5/16", [0.3125], None, None, 16, 0.0, "imperial"),
        # float32 values are dyadic: an unlimited budget favors 2^24.
        ("both-unlimited-dyadic", [0.3], None, None, 16_777_216, 0.0, "imperial"),
        ("overflow-cutoff", [1e9], "metric", None, 1, 0.0, "metric"),
        # Edge cases.
        ("negative-eighth", [-0.125], "metric", None, 1000, 0.0, "metric"),
        ("negative-5/16", [-0.3125], "imperial", None, 16, 0.0, "imperial"),
        ("integers-imperial", [3.0, -7.0, 0.0], "imperial", None, 1, 0.0, "metric"),
        ("max-factor-one", [0.4, 1.6, -2.5], None, 1, 1, 0.43333332737286884, "metric"),
        ("max-factor-50", [0.125], "metric", 50, 10, 0.024999999999999994, "metric"),
        ("overflow-half-metric", [1e9, 0.5], "metric", None, 1, 0.25, "metric"),
        ("overflow-half-imperial", [1e9, 0.5], "imperial", None, 2, 0.0, "imperial"),
        ("overflow-half-both", [1e9, 0.5], None, None, 2, 0.0, "imperial"),
        ("overflow-cutoff-imperial", [1e9], "imperial", None, 1, 0.0, "metric"),
        ("largest-below-int32", [2147483520.0], None, None, 1, 0.0, "metric"),
    ]

    BEST_FACTOR_SYSTEMS = [None, "metric", "imperial"]
    BEST_FACTOR_INVALID_UNIT_SYSTEMS = ["cubits", "", " metric", 0, 1, 1.5]
    BEST_FACTOR_INVALID_MAX_FACTORS = [0, -1, -(2**31)]
    BEST_FACTOR_INVALID_NUM_THREADS = [-1, -8]
    BEST_FACTOR_OVERFLOW_VALUES = [[3e9], [0.5, -3e9], [2147483648.0], [-2147483648.0]]
    BEST_FACTOR_THREAD_COUNTS = [0, 1, 2, 3, 8, 64]

    # Property test on `property_values`: expected factor and result system
    # for each (unit_system, budget).
    BEST_FACTOR_PROPERTY_BUDGETS = [10, 1000, 1_000_000]
    BEST_FACTOR_PROPERTY_EXPECTED = {
        ("metric", 10): (10, "metric"),
        ("metric", 1000): (1000, "metric"),
        ("metric", 1_000_000): (1_000_000, "metric"),
        ("imperial", 10): (9, "imperial"),
        ("imperial", 1000): (972, "imperial"),
        ("imperial", 1_000_000): (524_288, "imperial"),
        (None, 10): (10, "metric"),
        (None, 1000): (1000, "metric"),
        (None, 1_000_000): (524_288, "imperial"),
    }

    # Determinism test data (1M values) and the auto-threshold boundary.
    DETERMINISM_SEED = 7
    DETERMINISM_SIZE = 1_000_000
    AUTO_PARALLEL_THRESHOLD = 100_000

    # Benchmarks: expected result on `large_values` per system choice, and the
    # 50M-value `huge_values` data set.
    BEST_FACTOR_LARGE_EXPECTED = {
        None: (2048, "imperial"),
        "metric": (1000, "metric"),
        "imperial": (2048, "imperial"),
    }
    HUGE_SEED = 1
    HUGE_SIZE = 50_000_000
    HUGE_LOW = -1e6
    HUGE_HIGH = 1e6
    HUGE_FACTOR = 1000
