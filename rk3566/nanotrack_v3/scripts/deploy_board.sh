#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "usage: $0 SSH_TARGET" >&2
  exit 2
fi

target=$1
project_root=$(cd "$(dirname "$0")/.." && pwd)
board_root=/home/cat/Project/SOT
release_name=${DEPLOY_RELEASE_NAME:-rk3566-yolov8-detector}
if [[ ! "$release_name" =~ ^[A-Za-z0-9._-]+$ ]]; then
  echo "DEPLOY_RELEASE_NAME contains unsupported characters" >&2
  exit 2
fi
deploy_root=$board_root/nanotrack_v3_longterm/releases/$release_name
ssh_args=()
if [[ -n "${SSH_CONTROL_PATH:-}" ]]; then
  ssh_args+=(-S "$SSH_CONTROL_PATH")
fi
if [[ -n "${SSH_PORT:-}" ]]; then
  if [[ ! "$SSH_PORT" =~ ^[0-9]+$ ]]; then
    echo "SSH_PORT must be numeric" >&2
    exit 2
  fi
  ssh_args+=(-p "$SSH_PORT")
fi

if ssh "${ssh_args[@]}" "$target" "test -e '$deploy_root'"; then
  echo "refusing to overwrite existing $deploy_root" >&2
  exit 1
fi

ssh "${ssh_args[@]}" "$target" "mkdir -p '$deploy_root'"
tar -C "$project_root" \
  --exclude=.git --exclude=build --exclude='build-*' \
  --exclude='__pycache__' --exclude='.pytest_cache' \
  --exclude='*.onnx' --exclude='*.log' \
  -cf - . | ssh "${ssh_args[@]}" "$target" "tar -xf - -C '$deploy_root'"

echo "deployed versioned isolated project to $target:$deploy_root"

