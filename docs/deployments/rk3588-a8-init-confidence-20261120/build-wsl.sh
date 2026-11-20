#!/usr/bin/env bash
set -euo pipefail
root=/home/wjm/viewpro_gimbal_zp/rk3566/gimbal_tracker
work=/home/wjm/a8-init-confidence-20261120
cd "$root"
test "$(sha256sum A8_libs/libviewlink.a | cut -d' ' -f1)" = fec1c4326051446a90890132b03834d89ba9763a73237f34adcb09904032c040
test "$(sha256sum CMakeLists.txt | cut -d' ' -f1)" = 8ff00279fb6cf9441040f8ae2e26a032426f0dcb39416d661c81cb783f2ddfb0
test "$(sha256sum ai_alg/CMakeLists.txt | cut -d' ' -f1)" = 4a87d0cc9d98022ef7d80204a459f9955710cabe6616c4d460ff6fb94abf40cc
(cd "$work/input" && sha256sum -c inputs.sha256)
test ! -e ai_alg/source/control_error_limiter.hpp
test ! -e ai_alg/source/diagnostic_trace.cpp
mapfile -t source_names < <(cd "$work/input/source" && find . -maxdepth 1 -type f -name '*.cpp' -printf '%f\n' | sort)
mapfile -t header_names < <(cd "$work/input/include" && find . -maxdepth 1 -type f -name '*.hpp' -printf '%f\n' | sort)
paths=(CMakeLists.txt ai_alg/CMakeLists.txt ai_alg/source/native-lib_impl.cpp build_A8/viewlink_app)
for name in "${source_names[@]}"; do paths+=("ai_alg/nanotrack/source/$name"); done
for name in "${header_names[@]}"; do paths+=("ai_alg/nanotrack/include/$name"); done
for path in "${paths[@]}"; do test -f "$path"; done
sha256sum "${paths[@]}" > "$work/original.sha256"
mkdir -p "$work/backup"
for path in "${paths[@]}"; do
  mkdir -p "$work/backup/$(dirname "$path")"
  cp -p "$path" "$work/backup/$path"
done
restore() {
  for path in "${paths[@]}"; do cp -p "$work/backup/$path" "$path"; done
  rm -f ai_alg/source/control_error_limiter.hpp ai_alg/source/diagnostic_trace.cpp
  sha256sum -c "$work/original.sha256" > "$work/restored-check.log"
}
trap restore EXIT
for name in "${source_names[@]}"; do cp "$work/input/source/$name" "ai_alg/nanotrack/source/$name"; done
for name in "${header_names[@]}"; do cp "$work/input/include/$name" "ai_alg/nanotrack/include/$name"; done
for name in native-lib_impl.cpp control_error_limiter.hpp diagnostic_trace.cpp; do
  cp "$work/input/$name" "ai_alg/source/$name"
done
python3 - <<'PYCODE'
from pathlib import Path
p = Path('ai_alg/CMakeLists.txt')
s = p.read_text()
assert 'add_library(AI STATIC' in s
p.write_text(s.replace('add_library(AI STATIC', 'add_library(AI STATIC\n  ${CMAKE_CURRENT_SOURCE_DIR}/source/diagnostic_trace.cpp', 1))
p = Path('CMakeLists.txt')
p.write_text(p.read_text() + '\nset_property(TARGET viewlink_app APPEND_STRING PROPERTY LINK_FLAGS " -Wl,--wrap=tracker_detector_init,--wrap=do_tracker_detector,--wrap=write_serial")\n')
PYCODE
BUILD_JOBS=4 ./run.sh --platform A8 > "$work/build.log" 2>&1
cp build_A8/viewlink_app "$work/viewlink_app"
file "$work/viewlink_app"
sha256sum "$work/viewlink_app" "$work/build.log"
nm_tool=/home/wjm/rk_cross_build_tools/aarch64-buildroot-linux-gnu_sdk-buildroot/bin/aarch64-buildroot-linux-gnu-nm
"$nm_tool" "$work/viewlink_app" | grep -E ' (__wrap_|do_track$|viewlink_make_pack$)' > "$work/wrappers.txt"
