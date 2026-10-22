import numpy as np


class Constants:
    """
    Constants used in unit testing
    """

    EPSILON = 0.01

    VALUES = [0.4, 1.6, -2.5]
    DISCRETIZED = [0, 2, -3]

    BENCHMARK_SIZE = 1_000_000

    EMPTY = np.array([], dtype=np.float32)
