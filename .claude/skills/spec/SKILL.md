---
name: spec
description: Draft a feature spec (specs/<slug>/spec.md) for this project from a high-level idea, asking only the questions that matter. Use when the user describes a feature they want built by the agent pipeline.
argument-hint: <high-level description of the feature>
disable-model-invocation: true
---

Turn this idea into a spec the `/implement-spec` pipeline can execute:

$ARGUMENTS

1. Read CLAUDE.md, `specs/_template/spec.md`, the public header `cpp/include/discretization/discretization.hpp` and the Python package `python/src/discretization/__init__.py`, so the spec fits the existing API.
2. Pick a short kebab-case slug for the feature. If `specs/<slug>/` exists, pick another.
3. Work out what is missing for the spec to be executable without guessing: the end goal and acceptance criteria, which test kinds are required, and constraints such as precision, performance and allowed dependencies. Ask the user about the gaps in **one** AskUserQuestion call (up to 4 questions, each with a recommended option first). Don't ask about anything you can decide from the code or conventions.
4. Write `specs/<slug>/spec.md` from the template. Keep the user's wording for the goal. Make each acceptance criterion observable and checkable (a concrete input and output, an error bound, or a timing). In "Required tests", tick the kinds that apply and describe what each must cover. Put remaining uncertainties in "Open questions".
5. Show the user the path, the acceptance criteria and the ticked test kinds. Tell them to edit the file if needed, then run `/implement-spec specs/<slug>/spec.md`.

Don't write any code or tests.
