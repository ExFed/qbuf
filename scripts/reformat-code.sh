#!/usr/bin/env bash

set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

cd "$PROJECT_ROOT_DIR"
git ls-files --cached --others --exclude-standard -- '**/*.cpp' '**/*.hpp' \
    | xargs -r clang-format -i --verbose
