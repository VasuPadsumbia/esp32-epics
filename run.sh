#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT"

usage() {
  cat <<'EOF'
Usage:
  ./run.sh build
  ./run.sh rebuild
  ./run.sh client [caClient args...]

Examples:
  ./run.sh build
  ./run.sh rebuild
  ./run.sh client --help
  ./run.sh client get led
  ./run.sh client put led 1
  ./run.sh client monitor ai0:mean --duration 5

Notes:
  - `./run.sh client ...` never builds; it only runs an already-built caClient.
  - If caClient is missing, run `./run.sh build` (or `./run.sh rebuild`).
EOF
}

cmd="${1:-build}"
shift || true

jobs="${JOBS:-}"
if [[ -z "$jobs" ]]; then
  if command -v nproc >/dev/null 2>&1; then
    jobs="$(nproc)"
  else
    jobs="4"
  fi
fi

find_client_exe() {
  if [[ -n "${EPICS_HOST_ARCH:-}" && -x "$ROOT/bin/$EPICS_HOST_ARCH/caClient" ]]; then
    echo "$ROOT/bin/$EPICS_HOST_ARCH/caClient"
    return 0
  fi

  local exe
  exe="$(find "$ROOT/bin" -maxdepth 2 -type f -name caClient -print -quit 2>/dev/null || true)"
  if [[ -n "$exe" && -x "$exe" ]]; then
    echo "$exe"
    return 0
  fi

  return 1
}

case "$cmd" in
  build)
    /usr/bin/make -j"$jobs" 1>&2
    ;;

  rebuild)
    /usr/bin/make clean 1>&2
    /usr/bin/make -j"$jobs" 1>&2
    ;;

  client)
    exe="$(find_client_exe)" || {
      echo "Error: could not find built caClient under $ROOT/bin" >&2
      echo "Run: ./run.sh build" >&2
      exit 2
    }
    exec "$exe" "$@"
    ;;

  -h|--help|help)
    usage
    ;;

  *)
    echo "Unknown command: $cmd" >&2
    usage >&2
    exit 2
    ;;
esac
