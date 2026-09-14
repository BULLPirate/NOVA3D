#!/usr/bin/env bash
# Build (if needed) and launch NOVA3D apps from the repo root.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${ROOT}/build"

if [[ ! -x "${BUILD}/bin/Nova3D.app/Contents/MacOS/Nova3D" ]]; then
  echo "Building…"
  cmake -S "${ROOT}" -B "${BUILD}"
  cmake --build "${BUILD}" --target Nova3D NovaEditor
fi

case "${1:-editor}" in
  editor|e)
    open "${BUILD}/bin/NovaEditor.app"
    ;;
  game|runtime|g)
    open "${BUILD}/bin/Nova3D.app"
    ;;
  test|t)
    "${BUILD}/bin/NovaTests"
    ;;
  *)
    echo "Usage: $0 [editor|game|test]"
    exit 1
    ;;
esac
