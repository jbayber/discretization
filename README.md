# Discretization

Tools to help discretizing with high accuracy a floating point problem.

The core is a C++20 library. It is unit tested with [GoogleTest](https://github.com/google/googletest) and exposed to Python through [pybind11](https://github.com/pybind/pybind11), which is tested with [pytest](https://docs.pytest.org/). CMake builds everything. For Python, CMake is driven by [scikit-build-core](https://scikit-build-core.readthedocs.io/).

## Requirements

| Tool | Version | Used for |
|---|---|---|
| C++ compiler | C++20 (Clang 10+, GCC 10+ or MSVC 2019 16.11+) | Building the library, its tests, and the Python extension |
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
enum class discretization::UnitSystem { Metric, Imperial };

int discretization::calculate_discretization_factor(const std::vector<float>& values, float epsilon,
                                                    UnitSystem unit_system = UnitSystem::Metric);
int discretization::calculate_discretization_factor(const float* values, std::size_t size, float epsilon,
                                                    UnitSystem unit_system = UnitSystem::Metric);

std::vector<int> discretization::discretize(const std::vector<float>& values, float epsilon,
                                            UnitSystem unit_system = UnitSystem::Metric);
void discretization::discretize(const float* values, std::size_t size, float epsilon, int* out,
                                UnitSystem unit_system = UnitSystem::Metric);
```

Each function has a `std::vector` version and a raw-buffer version (pointer and size). The vector version calls the raw-buffer version.

### Behavior

`calculate_discretization_factor` returns the **smallest** candidate factor `f` of the chosen unit system that *fits* the values. For a value `v` (converted to `double`), let `q = round(v · f)`, with halfway cases rounded away from zero. `f` fits if `|q / f − v| ≤ epsilon` for every value. `discretize` returns `q` for every value, using exactly that factor. Empty input gives factor `1` and an empty result.

Candidate factors, tried in ascending order:

- **Metric** (default): `1, 10, 100, …, 10^9`.
- **Imperial**: every `2^a · 3^b` that fits in `int32` (328 numbers, up to `2066242608`): `1, 2, 3, 4, 6, 8, 9, 12, 16, 18, 24, 27, 32, 36, 48, 54, 64, 72, 81, 96, …`. This covers binary inch fractions (1/2 … 1/64 in), 12 in/ft, 3 ft/yd, 36 in/yd and 16 oz/lb.

Errors:

- `epsilon` that is NaN or `≤ 0`, or any NaN or ±inf value: `std::invalid_argument` (Python `ValueError`).
- No candidate fits, or `round(v · f)` overflows `int32` before a candidate fits: `std::out_of_range` (Python `ValueError`). The search stops at the first overflow, because larger factors only make it worse.

Every finite float32 is a dyadic rational, so in imperial a fitting power of two always exists unless the values overflow (for example, `0.1f` fits at `2^27`). "No candidate fits" is therefore mainly a metric error, and imperial fails mainly through overflow.

Python:

```python
import numpy as np
import pandas as pd
from discretization import UnitSystem, calculate_discretization_factor, discretize

values = np.array([0.4, 1.6, -2.5], dtype=np.float32)
calculate_discretization_factor(values, 0.01)  # 10
discretize(values, 0.01)  # array([  4,  16, -25], dtype=int32)

calculate_discretization_factor(values, 0.01, "imperial")  # 32
# array([ 13,  51, -80], dtype=int32)
discretize(values, 0.01, unit_system=UnitSystem.IMPERIAL)

df = pd.DataFrame({"x": [0.4, 1.6, -2.5]})
discretize(df["x"].to_numpy(np.float32), 0.01)
```

`unit_system` defaults to `"metric"`. It accepts `"metric"` / `"imperial"` (case-insensitive) or `UnitSystem.METRIC` / `UnitSystem.IMPERIAL`. Anything else raises `ValueError`.

The Python functions call the raw-buffer C++ versions:

- A contiguous 1-D `float32` NumPy array is passed to C++ **without copying**.
- Anything else (lists, `float64`, strided views, pandas Series) is converted to that format once. For the fastest path, convert it yourself, for example with `series.to_numpy(np.float32)`.
- The result is written directly into a new `int32` NumPy array.
- The GIL is released during the computation.
- Input that isn't 1-D raises `ValueError`. Both C++ exceptions above surface as `ValueError`.

## Agent workflow

Features can be built by [Claude Code](https://claude.com/claude-code) agents from a short spec. You describe what you want, which tests it needs, and the end goal. The agents plan, write the tests, implement, verify and review.

### 1. Write a spec

```
/spec add a real calculate_discretization_factor: smallest power of 10 such that every value * factor is within epsilon of an integer
```

Claude asks a few targeted questions (acceptance criteria, required test kinds, constraints), then writes `specs/<slug>/spec.md`. You can also copy [specs/_template/spec.md](specs/_template/spec.md) and fill it in yourself. A spec has these sections:

| Section | What you write |
|---|---|
| Goal | What you want, at a high level |
| End goal / acceptance criteria | Checkable outcomes. The work is accepted only if all of them hold. |
| API | New or changed functions, as precise as you like |
| Required tests | Tick the kinds you need (C++ unit, Python unit, edge cases, property/accuracy, input types, benchmarks, regression) and say what each must cover |
| Constraints / Out of scope | Limits the agents must respect |
| Delivery | Commit on a branch, or also push and open a PR |

### 2. Run it

```
/implement-spec specs/<slug>/spec.md
```

The orchestrator asks its questions once, up front, then runs without stopping:

| Step | Agent ([.claude/agents/](.claude/agents/)) | Output |
|---|---|---|
| Check the spec, create `feature/<slug>`, confirm the baseline is green | orchestrator | Branch, committed spec |
| Plan API changes, files, test cases and steps | `planner` (read-only) | `specs/<slug>/plan.md` |
| Write the tests from the spec, before any implementation | `test-writer` | Failing tests, committed |
| Implement until `make test` passes | `implementer` | Implementation |
| Format and lint (clang-format, ruff) until `make check` passes, without changing behavior | `formatter` | Formatted code, committed with the implementation |
| Review against the spec, conventions and numerical correctness, looping back to the implementer and formatter on blocking findings (max 3 rounds) | `reviewer` (read-only) | Review findings |
| Final verification, report, optional push and PR | orchestrator | `specs/<slug>/report.md` |

The test-writer and implementer are separate agents, so tests come from the spec, not from the code. The formatter runs on a smaller, cheaper model (Haiku): it only applies the project's formatters and fixes lint that can't change behavior, and reports anything else back to the implementer. Agents may never weaken or delete a test to make it pass.

The run stops and asks you only if something blocks it: an ambiguous spec, a dirty working tree, a red baseline, a disputed test the spec doesn't settle, or review findings still open after 3 rounds.

For an unattended run, switch Claude Code to accept-edits mode (Shift+Tab) before starting. Builds, tests, formatting and local git commands are pre-approved in [.claude/settings.json](.claude/settings.json). `git push` and `gh pr create` always ask, and force-push and `git reset --hard` are blocked.

You can also call the formatter on its own at any time, for example when `make check` fails: ask Claude to "use the formatter agent".

### How it's configured

| File | Purpose |
|---|---|
| [CLAUDE.md](CLAUDE.md) | Project rules every agent follows: commands, conventions, definition of done |
| [.claude/agents/](.claude/agents/) | `planner`, `test-writer`, `implementer`, `formatter`, `reviewer` |
| [.claude/skills/](.claude/skills/) | The `/spec` and `/implement-spec` commands |
| [.claude/settings.json](.claude/settings.json) | Pre-approved commands, plus a hook that formats every file an agent edits |
| [specs/](specs/) | One folder per feature: `spec.md` (yours), `plan.md` and `report.md` (written by the agents) |

New C++ test files in `tests_cpp/` are picked up by CMake automatically, so agents don't need to edit `CMakeLists.txt` to add tests.

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
.claude/                                        Claude Code agents, commands, settings and hooks
CLAUDE.md                                       Rules for Claude Code agents
specs/                                          Feature specs for the agent workflow (_template/spec.md)
Makefile                                        install / format / lint / check / test targets
```

## License

[MIT](LICENSE)
