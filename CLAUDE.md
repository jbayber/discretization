# Discretization — instructions for Claude

C++20 library (`cpp/`) exposed to Python with pybind11 (`python/`). C++ is tested with GoogleTest (`tests_cpp/`), Python with pytest (`tests_py/`). CMake builds everything; scikit-build-core drives CMake for the Python package. See [README.md](README.md) for the full setup.

## Commands

Activate the environment first: `source .venv/bin/activate`. If `.venv` is missing, run `uv venv -p 3.13 && source .venv/bin/activate && make install`.

| Command | Use |
|---|---|
| `make install` | Rebuild the Python extension. **Required after any C++ change** before running Python tests. |
| `make test-cpp` | Configure, build and run the GoogleTest tests |
| `make test-py` | Run the pytest tests (against the installed extension) |
| `make test` | Both |
| `make format` | ruff + clang-format, rewrites files |
| `make check` | Formatting and lint checks, no rewrite (what CI runs) |

Run a single test: `./build/cpp/discretization_tests --gtest_filter='Suite.Name'` or `pytest tests_py/test_x.py::TestClass::test_name`.

## Layout and conventions

- **Public C++ API:** `cpp/include/discretization/discretization.hpp`, namespace `discretization`. Every function has a `std::vector` version and a raw-buffer version (`const float *values, std::size_t size, ...`). The vector version delegates to the raw-buffer version, which holds the real logic.
- **Python bindings:** `python/src/bindings.cpp` binds only the raw-buffer versions, takes `py::array_t<float, c_style | forcecast>` (zero copy for contiguous float32), returns NumPy arrays, rejects non-1-D input with `ValueError`, and releases the GIL around the C++ call. Re-export new functions in `python/src/discretization/__init__.py` (`__all__`).
- **C++ tests:** `tests_cpp/test_<topic>.cpp`, GoogleTest. Every `tests_cpp/*.cpp` is picked up automatically by CMake. Test both the vector and the raw-buffer versions.
- **Python tests:** MAGE style (as in Memgraph's `mage/python/tests`): one `tests_py/test_<function>.py` per function, tests grouped in plain classes (`class TestX:`, no `unittest.TestCase`), shared data in `tests_py/constants.py` (`Constants`), fixtures in `tests_py/conftest.py`, variations with `@pytest.mark.parametrize`, benchmarks with the `benchmark` fixture in a `TestXBenchmark` class.
- **Style:** C++ follows `.clang-format` (Memgraph style, 120 columns, `float *p`). Python follows ruff (88 columns). Don't format by hand: a hook formats edited files, and `make format` formats everything.
- Use fixed seeds for any randomized test data.

## Definition of done

A change is done only when all of these hold:

1. `make format` leaves no diff, and `make check` passes.
2. `make install && make test` passes, with no skipped or xfailed tests that weren't there before.
3. Every new or changed public function has C++ and Python tests.
4. README's API section is updated if the public API or behavior changed.

## Rules

- Never weaken, delete or skip an existing test to make a change pass. If a test looks wrong, stop and report it.
- Don't add dependencies (C++ or Python) unless the spec allows it.
- Don't push, open PRs, or touch `main` unless explicitly asked. Work on a feature branch.
- Keep the C++ core free of Python: only `python/src/bindings.cpp` includes pybind11.

## Agent workflow

Features are described as specs in `specs/<slug>/spec.md` (template: `specs/_template/spec.md`).

- `/spec <idea>` turns a high-level idea into a spec.
- `/implement-spec specs/<slug>/spec.md` runs the full pipeline with the subagents in `.claude/agents/`: planner → test-writer → implementer → formatter → reviewer, then writes `specs/<slug>/report.md`.
- Use the `formatter` agent whenever `make check` fails. It fixes formatting and lint without changing behavior.
