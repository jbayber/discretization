#!/usr/bin/env bash
# PostToolUse hook: format the file Claude just edited, with the project's pinned tools.
# Never blocks the edit: formatting problems are left for `make check`.
set -u

file=$(jq -r '.tool_input.file_path // empty')
[ -n "$file" ] && [ -f "$file" ] || exit 0

bin="${CLAUDE_PROJECT_DIR:-.}/.venv/bin"
case "$file" in
  *.cpp | *.hpp | *.h | *.cc)
    [ -x "$bin/clang-format" ] && "$bin/clang-format" -i "$file"
    ;;
  *.py)
    [ -x "$bin/ruff" ] && "$bin/ruff" check --select I --fix --quiet "$file" && "$bin/ruff" format --quiet "$file"
    ;;
esac
exit 0
