#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "usage: $0 SSH_TARGET" >&2
  exit 2
fi

target=$1
project_root=$(cd "$(dirname "$0")/.." && pwd)
board_root=/home/cat/Project/SOT

if ssh "$target" "test -e '$board_root/nanotrack_v3'"; then
  echo "refusing to overwrite existing $board_root/nanotrack_v3" >&2
  exit 1
fi

tar -C "$(dirname "$project_root")" -cf - "$(basename "$project_root")" | \
  ssh "$target" "mkdir -p '$board_root' && tar -xf - -C '$board_root'"

