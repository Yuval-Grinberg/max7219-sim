#!/usr/bin/env sh
# Build and run an Arduino sketch in the MAX7219 simulator (Linux, macOS, MSYS2 shell).
#   ./sim.sh path/to/MySketch [simulator options]
set -e
if [ -z "$1" ]; then
  echo "usage: ./sim.sh SKETCH_FOLDER_OR_INO [options]   e.g. ./sim.sh examples/Demo"
  exit 1
fi
here=$(cd "$(dirname "$0")" && pwd)
sketch=$1; shift
name=$(basename "${sketch%/}"); name=${name%.ino}
build="$here/build/$name"
gen=""
command -v ninja >/dev/null 2>&1 && gen="-G Ninja"
cmake -S "$here" -B "$build" $gen -DSKETCH="$sketch" >/dev/null
cmake --build "$build" -j 4
exe=$(ls "$build"/*_sim "$build"/*_sim.exe 2>/dev/null | head -n 1)
exec "$exe" "$@"
