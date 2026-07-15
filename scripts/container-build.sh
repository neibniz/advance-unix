#!/usr/bin/env bash
set -euo pipefail

readonly image="ghcr.io/neibniz/clang-dev:7ee244e55f19"
repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)"

case "${repo_root}" in
  /data/*) ;;
  *)
    printf 'error: this script may only mount a project below /data (got %s)\n' \
      "${repo_root}" >&2
    exit 2
    ;;
esac

exec docker run --rm \
  --pull=never \
  --network=none \
  --mount "type=bind,src=${repo_root},dst=/workspace" \
  --workdir /workspace \
  "${image}" \
  cmake --workflow --preset verify
