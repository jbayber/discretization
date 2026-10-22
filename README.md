# Discretization

Tools to help discretizing with high accuracy a floating point problem.

The core is a C++17 library. It is unit tested with [GoogleTest](https://github.com/google/googletest) and exposed to Python through [pybind11](https://github.com/pybind/pybind11), which is tested with [pytest](https://docs.pytest.org/). CMake builds everything. For Python, CMake is driven by [scikit-build-core](https://scikit-build-core.readthedocs.io/).

## Requirements

| Tool | Version | Used for |
|---|---|---|
| C++ compiler | C++17 (Clang, GCC or MSVC) | Building the library, its tests, and the Python extension |
| [CMake](https://cmake.org/download/) | 3.20 or later | Build system. Optional: `uvx cmake` is used when it isn't installed. |
| [uv](https://docs.astral.sh/uv/getting-started/installation/) | recent | Python versions, virtual environments, and packages |
| Python | 3.9 or later (3.13 recommended) | Python bindings and tests. uv can install it for you. |
| Network access | first build only | CMake downloads GoogleTest. pip downloads pybind11, NumPy, pandas, pytest (and its plugins), ruff and clang-format. |

## Setup

### 1. Install the tools

**macOS**

```bash
xcode-select --install       # Clang compiler (skip if already installed)
brew install uv cmake        # cmake is optional, see Requirements
```

**Linux (Debian/Ubuntu)**

```bash
sudo apt-get install build-essential cmake
curl -LsSf https://astral.sh/uv/install.sh | sh
```

Check the tools:

```bash
c++ --version
uv --version
cmake --version              # or: uvx cmake --version
```

### 2. Create the Python environment

From the repository root:

```bash
uv venv -p 3.13              # creates .venv/, downloading Python 3.13 if needed
source .venv/bin/activate
```

Activate the environment (`source .venv/bin/activate`) in every new shell before running `make` or `pytest`.

### 3. Build and install the package

```bash
make install                 # same as: uv pip install -e ".[dev]"
```

This command compiles the C++ library and the pybind11 extension (`discretization._core`) with CMake. It then installs the `discretization` package in editable mode, together with the development tools.

Available extras:

| Extra | Contents | Command |
|---|---|---|
| *(none)* | The package and NumPy | `uv pip install -e .` |
| `test` | + pytest (with pytest-cov, pytest-timeout and pytest-benchmark) and pandas | `uv pip install -e ".[test]"` |
| `dev` | + the `test` extra, ruff and clang-format | `uv pip install -e ".[dev]"` |

Without uv, use `python -m venv .venv` and `pip install -e ".[dev]"` instead.

### 4. Check the setup

```bash
make test
```

This builds and runs the C++ tests, then runs the Python tests. Both should pass.

## Building

### Python package

`make install` builds the extension and installs the package.

**Rerun `make install` after every change to a C++ file** (`cpp/` or `python/src/bindings.cpp`). The installed extension is not rebuilt automatically. Changes to Python files take effect immediately.

To build a wheel to distribute:

```bash
uv build --wheel             # writes dist/discretization-<version>-<tags>.whl
```

### C++ library only

The C++ library can be built on its own, without Python:

```bash
cmake -S . -B build/cpp                 # configure (add -DCMAKE_BUILD_TYPE=Release for optimized builds)
cmake --build build/cpp -j              # build
```

This builds the static library `libdiscretization.a` and the test executable `build/cpp/discretization_tests`.

CMake options:

| Option | Default | Description |
|---|---|---|
| `DISCRETIZATION_BUILD_TESTS` | `ON` (`OFF` when built by pip) | Build the GoogleTest tests in `tests_cpp/` |
| `DISCRETIZATION_BUILD_PYTHON` | `OFF` (`ON` when built by pip) | Build the pybind11 extension |

For example, `cmake -S . -B build/cpp -DDISCRETIZATION_BUILD_TESTS=OFF` builds only the library.

Build outputs go to `build/`, which is git-ignored. To start from a clean state, delete `build/` and rerun `make install`.

## Testing

| Command | What it runs |
|---|---|
| `make test` | Both suites: C++ first, then Python |
| `make test-cpp` | C++ tests in `tests_cpp/` (configures and builds first, then runs CTest) |
| `make test-py` | Python tests in `tests_py/` (same as `pytest`) |

C++ tests use [GoogleTest](https://github.com/google/googletest), registered with CTest. Python tests use [pytest](https://docs.pytest.org/). 
GoogleTest only runs C++, so the Python module is tested from Python.

### Python test layout

The layout follows MAGE's `python/tests/`:

```
tests_py/
    __init__.py
    constants.py                              Shared test data (class Constants)
    conftest.py                               pytest fixtures (NumPy arrays, pandas Series/DataFrame, large benchmark input)
    test_calculate_discretization_factor.py   class TestCalculateDiscretizationFactor
    test_discretize.py                        class TestDiscretize, TestDiscretizeInputTypes, TestDiscretizeBenchmark
```

- There is one test file per function, with tests grouped in plain pytest classes (`class TestX:`, no `unittest.TestCase`).
- Shared inputs come from fixtures in `conftest.py` and from `Constants`. Variations use `@pytest.mark.parametrize`.
- Every `pytest` run also does the following (configured in `[tool.pytest.ini_options]` in `pyproject.toml`):
  - Runs verbosely (`-vv`) and stops after 10 failures (`--maxfail=10`).
  - Fails any test that runs longer than 60 s (pytest-timeout).
  - Reports coverage of the Python package in the terminal and as HTML in `build/coverage/` (pytest-cov).
  - Times the benchmark tests and prints a comparison table (pytest-benchmark). For example, it compares a `float32` array passed without copying against a `float64` array that needs conversion.

### Running a subset of tests

```bash
# C++: by CTest name pattern, or with GoogleTest filters on the executable
ctest --test-dir build/cpp -R Discretize --output-on-failure
./build/cpp/discretization_tests --gtest_filter='Discretize.*'

# Python: by file, test name, or keyword
pytest tests_py/test_discretize.py
pytest tests_py/test_discretize.py::TestDiscretize::test_empty
pytest -k series

# Python: skip or run only the benchmarks
pytest --benchmark-skip
pytest --benchmark-only
```

Remember that the Python tests run against the **installed** extension. If you changed C++ code, rerun `make install` before `make test-py`.

## Development

| Command | What it does |
|---|---|
| `make format` | Format the Python and C++ code (rewrites files) |
| `make format-py` | Sort imports and format the Python code with ruff |
| `make format-cpp` | Format the C++ code with clang-format |
| `make lint` | Report Python lint issues |
| `make check` | Check Python and C++ formatting, and Python lint, without changing files |
| `make check-py` / `make check-cpp` | The same checks for one language only |
| `make test` | Run the C++ and Python test suites |

- **Python:** ruff is configured in `pyproject.toml`: 88-character lines, with pyflakes, pycodestyle and import-sorting rules.
- **C++:** clang-format is configured in [.clang-format](.clang-format).
- clang-format is installed from PyPI by `make install` and pinned in `pyproject.toml`, so everyone formats with the same version. Different clang-format versions can format the same code differently.

GitHub Actions ([.github/workflows/ci.yml](.github/workflows/ci.yml)) runs `make check` and `make test` on every pull request and on every push to `main`. `make check` covers Python and C++ formatting and Python lint. `make test` builds and runs the GoogleTest tests, then the pytest tests. A failing check fails the run. CI never reformats code, so run `make format` before pushing.

## API

C++ (`#include <discretization/discretization.hpp>`):

```cpp
int discretization::calculate_discretization_factor(const std::vector<float>& values, float epsilon);
int discretization::calculate_discretization_factor(const float* values, std::size_t size, float epsilon);

std::vector<int> discretization::discretize(const std::vector<float>& values, float epsilon);
void discretization::discretize(const float* values, std::size_t size, float epsilon, int* out);
```

Each function has a `std::vector` version and a raw-buffer version (pointer and size). The vector version calls the raw-buffer version.

Python:

```python
import numpy as np
import pandas as pd
from discretization import calculate_discretization_factor, discretize

values = np.array([0.4, 1.6, -2.5], dtype=np.float32)
discretize(values, 0.01)  # array([ 0,  2, -3], dtype=int32)
calculate_discretization_factor(values, 0.01)  # 1

df = pd.DataFrame({"x": [0.4, 1.6, -2.5]})
discretize(df["x"].to_numpy(np.float32), 0.01)
```

The Python functions call the raw-buffer C++ versions:

- A contiguous 1-D `float32` NumPy array is passed to C++ **without copying**.
- Anything else (lists, `float64`, strided views, pandas Series) is converted to that format once. For the fastest path, convert it yourself, for example with `series.to_numpy(np.float32)`.
- The result is written directly into a new `int32` NumPy array.
- The GIL is released during the computation.
- Input that isn't 1-D raises `ValueError`.

Current behavior is a placeholder. The factor is always `1`. `discretize` rounds each value to the nearest integer, and rounds halfway cases away from zero (`2.5` → `3`, `-2.5` → `-3`).

## Troubleshooting

- **`cmake: command not found`**: install CMake (see [Setup](#1-install-the-tools)). `make test-cpp` falls back to `uvx cmake` automatically, but running CMake by hand needs `uvx cmake ...`.
- **Python tests don't reflect a C++ change**: rerun `make install`.
- **`ModuleNotFoundError: No module named 'discretization'`**: activate the environment (`source .venv/bin/activate`), then run `make install`.
- **The first build fails while downloading**: the first configure needs network access to download GoogleTest and the Python build dependencies.

## Project structure

```
cpp/
    include/discretization/discretization.hpp   Public C++ API
    src/discretization.cpp                      C++ implementation
python/
    src/bindings.cpp                            pybind11 bindings (module discretization._core)
    src/discretization/                         Python package
tests_cpp/                                      C++ unit tests (GoogleTest)
tests_py/                                       Python unit tests (pytest, MAGE-style layout)
CMakeLists.txt                                  C++ library, tests and Python extension
pyproject.toml                                  Python package metadata, build backend, pytest/ruff configuration
.clang-format                                   C++ formatting style
Makefile                                        install / format / lint / check / test targets
```

## License

[MIT](LICENSE)
