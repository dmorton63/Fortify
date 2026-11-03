#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="${1:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"

INCLUDE_DIR="$PROJECT_ROOT/include"
SOURCE_DIR="$PROJECT_ROOT/src"
BUILD_DIR="$PROJECT_ROOT/build"
OBJ_DIR="$BUILD_DIR/obj"
LINKER_SCRIPT="$BUILD_DIR/linker.ld"
KERNEL_OUT="$BUILD_DIR/fortify.elf"

CLANG="${CLANG:-clang++}"
LD="${LD:-ld.lld}"

mkdir -p "$OBJ_DIR"

mapfile -t sources < <(find "$SOURCE_DIR" -type f -name '*.cpp' | sort)
if [[ ${#sources[@]} -eq 0 ]]; then
    echo "No C++ sources found under $SOURCE_DIR" >&2
    exit 1
fi

objects=()
for source in "${sources[@]}"; do
    rel="${source#"$SOURCE_DIR"/}"
    obj="$OBJ_DIR/${rel%.cpp}.o"
    mkdir -p "$(dirname "$obj")"

    "$CLANG" \
        -target x86_64-unknown-none \
        -ffreestanding \
        -fno-stack-protector \
        -fno-pic \
        -mcmodel=kernel \
        -mno-red-zone \
        -Wall \
        -Wextra \
        -O2 \
        -I "$INCLUDE_DIR" \
        -fno-exceptions \
        -fno-rtti \
        -fno-use-cxa-atexit \
        -fno-threadsafe-statics \
        -fno-aligned-allocation \
        -c "$source" \
        -o "$obj"

    objects+=("$obj")
done

"$LD" \
    -nostdlib \
    -z max-page-size=0x1000 \
    -T "$LINKER_SCRIPT" \
    -o "$KERNEL_OUT" \
    "${objects[@]}"

echo "Built kernel: $KERNEL_OUT"
