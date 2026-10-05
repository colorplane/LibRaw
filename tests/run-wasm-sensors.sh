#!/usr/bin/env bash
set -euo pipefail

# Use the exact archives and compiler flags shipped by the custom wrapper.
wasm_root="${1:?Usage: bash tests/run-wasm-sensors.sh /path/to/LibRaw-Wasm [output-directory]}"
test_root="$(cd "$(dirname "$0")" && pwd)"
output_directory="${2:-$(mktemp -d)}"
mkdir -p "$output_directory"
for test_name in unpacked_sensor sony_arw2_sensor; do
  em++ -O3 -flto -msimd128 -pthread -sSTACK_SIZE=1MB \
    -sUSE_LIBJPEG=1 -sUSE_ZLIB=1 -sALLOW_MEMORY_GROWTH=1 \
    -sENVIRONMENT=node -sDISABLE_EXCEPTION_CATCHING=0 \
    -I"$wasm_root/includes" -I"$wasm_root/LibRawSource" \
    "$test_root/$test_name.cpp" "$wasm_root/libs/libraw.a" \
    "$wasm_root/libs/liblcms2.a" -o "$output_directory/$test_name.cjs"
  node "$output_directory/$test_name.cjs"
done
