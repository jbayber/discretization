.PHONY: install format format-py format-cpp lint check check-py check-cpp test test-py test-cpp

BUILD_DIR ?= build/cpp
CMAKE ?= $(shell command -v cmake >/dev/null 2>&1 && echo cmake || echo uvx cmake)
CPP_SOURCES := $(shell find cpp python/src tests_cpp -name '*.cpp' -o -name '*.hpp')
CTEST ?= $(shell command -v ctest >/dev/null 2>&1 && echo ctest || echo uvx --from cmake ctest)

install:  ## Build and install the package (with its C++ extension) plus dev tools
	uv pip install -e ".[dev]"

format: format-py format-cpp  ## Format the Python and C++ code (rewrites files)

format-py:  ## Sort imports and format the Python code with ruff
	ruff check --select I --fix .
	ruff format .

format-cpp:  ## Format the C++ code with clang-format
	clang-format -i $(CPP_SOURCES)

lint:  ## Report lint issues
	ruff check .

check: check-py check-cpp  ## Verify formatting and lint without changing files (for CI)

check-py:  ## Verify Python formatting and lint
	ruff format --check .
	ruff check .

check-cpp:  ## Verify C++ formatting
	clang-format --dry-run --Werror $(CPP_SOURCES)

test: test-cpp test-py  ## Run the C++ and Python test suites

test-py:  ## Run the Python tests (tests_py)
	pytest

test-cpp:  ## Build and run the C++ tests (tests_cpp)
	$(CMAKE) -S . -B $(BUILD_DIR)
	$(CMAKE) --build $(BUILD_DIR) -j
	$(CTEST) --test-dir $(BUILD_DIR) --output-on-failure
