# <Feature title>

<!--
Fill in what you know; leave a section as "TBD" if you don't care.
Run `/spec <idea>` to have Claude draft this file for you,
then `/implement-spec specs/<slug>/spec.md` to implement it.
-->

## Goal

<!-- What should exist when this is done, and why. One or two paragraphs, high level is fine. -->

## End goal / acceptance criteria

<!-- Observable, checkable outcomes. The work is accepted only if every item holds. -->

- [ ] ...

## API

<!-- New or changed functions, in C++ and/or Python. Signatures if you have them, otherwise a description. -->

## Required tests

<!-- Tick the kinds you need and describe what each must cover. -->

- [ ] **C++ unit tests** (GoogleTest, `tests_cpp/`):
- [ ] **Python unit tests** (pytest, `tests_py/`):
- [ ] **Edge cases** (empty input, NaN/inf, huge values, negative values, ...):
- [ ] **Property / accuracy tests** (randomized with a fixed seed, e.g. "error ≤ epsilon for 10k random inputs"):
- [ ] **Python input types** (NumPy float32/float64, lists, strided views, pandas Series/DataFrame columns):
- [ ] **Benchmarks** (pytest-benchmark; give a target if you have one, e.g. "< 5 ms for 1M values"):
- [ ] **Regression tests** for:

## Constraints

<!-- Performance, memory, precision, allowed dependencies, things that must not change. -->

## Out of scope

<!-- What the agents must NOT do. -->

## Delivery

- [x] Commit on a feature branch `feature/<slug>`
- [ ] Push the branch and open a pull request

## Open questions

<!-- Anything you're unsure about. The pipeline asks you about these before starting. -->
