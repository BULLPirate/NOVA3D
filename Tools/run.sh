#!/usr/bin/env bash
# Build (if needed) and launch NOVA3D apps from the repo root.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${ROOT}/build"

ensure_cmake() {
  if [[ ! -f "${BUILD}/CMakeCache.txt" ]]; then
    echo "Configuring CMake…"
    cmake -S "${ROOT}" -B "${BUILD}"
  fi
}

build_target() {
  ensure_cmake
  echo "Building ${1}…"
  cmake --build "${BUILD}" --target "$1"
}

app_bin() {
  echo "${BUILD}/bin/${1}.app/Contents/MacOS/${1}"
}

launch_mac_app() {
  local name="$1"
  local app="${BUILD}/bin/${name}.app"
  local bin
  bin="$(app_bin "${name}")"
  if [[ ! -x "${bin}" ]]; then
    echo "ERROR: ${bin} not found. Build failed?"
    exit 1
  fi
  echo "Launching ${app}"
  echo "Log: ~/Library/Logs/NOVA3D.log"
  # -n: new instance; fallback to direct exec if Finder open fails
  if ! open -n "${app}" 2>/dev/null; then
    echo "open failed — starting binary directly…"
    exec "${bin}"
  fi
}

case "${1:-editor}" in
  build|b)
    ensure_cmake
    cmake --build "${BUILD}" --target NovaEditor Nova3D NovaTests
    ;;
  editor|e)
    build_target NovaEditor
    launch_mac_app NovaEditor
    ;;
  game|runtime|g)
    build_target Nova3D
    launch_mac_app Nova3D
    ;;
  test|t)
    build_target NovaTests
    "${BUILD}/bin/NovaTests"
    ;;
  play-scene)
    build_target Nova3D
    SCENE="${2:-${ROOT}/Assets/Scenes/demo.scene.json}"
    exec "$(app_bin Nova3D)" --scene "${SCENE}"
    ;;
  knight|bandits)
    build_target Nova3D
    GAME="${HOME}/Desktop/KnightBandits"
    exec "$(app_bin Nova3D)" --project "${GAME}"
    ;;
  game-project|gp)
    build_target Nova3D
    exec "$(app_bin Nova3D)" --project "${ROOT}"
    ;;
  *)
    echo "Usage: $0 [editor|game|test|build|play-scene PATH|game-project|knight]"
    exit 1
    ;;
esac
