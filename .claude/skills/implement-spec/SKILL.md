---
name: implement-spec
description: Implement a feature spec end to end with the planner, test-writer, implementer, formatter and reviewer subagents, then verify, commit on a feature branch and write a report. Use when the user runs /implement-spec with a spec path.
argument-hint: specs/<slug>/spec.md
disable-model-invocation: true
---

Implement the spec at `$ARGUMENTS` autonomously. You are the orchestrator: delegate the work to the subagents, check their output, and keep going until the definition of done in CLAUDE.md holds. Only stop to ask the user at step 1, or when a step below says to.

Keep `specs/<slug>/report.md` up to date as you go (create it at step 2), so that progress is visible and the run can be resumed.

## 1. Check the spec

- Read CLAUDE.md and the spec. If the path doesn't exist, stop and suggest `/spec`.
- The spec must have a goal, at least one checkable acceptance criterion, and at least one ticked test kind. If not, or if "Open questions" contains anything that would change what gets built, ask the user in **one** AskUserQuestion call. Record the answers in the spec's "Open questions" section.
- From here on, run without asking, except where a step says to stop.

## 2. Prepare the branch

- `git status` must be clean (ignoring the spec itself). If not, stop and ask.
- `git checkout main && git pull --ff-only` if there's a remote, then `git checkout -b feature/<slug>`. If the branch exists, ask whether to resume on it.
- Make sure `.venv` exists and is current: `source .venv/bin/activate && make install`. Create it as described in CLAUDE.md if missing. Run `make test` once to confirm the baseline is green; if it isn't, stop and report.
- Commit the spec: `git add specs/<slug>/spec.md && git commit -m "Add spec: <title>"`.

## 3. Plan: `planner` subagent

Give it the spec path. Save its plan to `specs/<slug>/plan.md`. If the plan has **BLOCKING** questions, ask the user, update the spec, and plan again.

## 4. Tests: `test-writer` subagent

Give it the spec path and the plan. Check its report: every acceptance criterion maps to a test, and failures are "not implemented yet", not compile or import errors in the tests. If not, send it back with the specific gap (at most twice). Commit: `Add tests: <title>`.

## 5. Implementation: `implementer` subagent

Give it the spec path, the plan and the test-writer's report. When it reports done, verify yourself with `make install && make test`. If something fails, send the output back to the implementer.

If the implementer reports a test it believes is wrong, decide from the spec. If the spec settles it, tell the losing side what to change. If it doesn't, stop and ask the user.

## 6. Formatting: `formatter` subagent

Run the formatter. If it reports issues left for the implementer, send them to the implementer, then run the formatter again (at most twice). Verify yourself: `make check` and `make install && make test`.

Commit: `Implement <title>`.

## 7. Review: `reviewer` subagent

Give it the spec path. On `CHANGES REQUIRED`:
1. Pass only the **BLOCKING** findings to the implementer.
2. Run the formatter.
3. Re-verify with `make check` and `make install && make test`.
4. Commit (`Address review: <title>`) and review again.

Allow at most 3 review rounds. If it still isn't approved, stop and report the open findings. Minor findings go into the report, not into more rounds.

## 8. Deliver

- Final check: `make format` leaves no diff, `make check` passes, `make install && make test` passes.
- Tick the acceptance criteria in the spec that are met, and commit together with the final report.
- If the spec's Delivery section ticks "open a pull request": `git push -u origin feature/<slug>`, then `gh pr create` with the report's summary as the body. Otherwise don't push.

## Report

`specs/<slug>/report.md` holds:

- **Status:** done / blocked, plus the branch name.
- **Summary** of what was built.
- **Acceptance criteria:** each one with ✅ or ❌ and the test(s) that prove it.
- **Tests added:** C++ and Python, with counts, and benchmark numbers if any.
- **Final results** of `make check` and `make test` (test counts).
- **Decisions and deviations** from the spec or plan, with reasons.
- **Reviewer's minor findings** not addressed.
- **Open questions / follow-ups.**

Finish by giving the user the branch, the acceptance-criteria table, and the path to the report.
