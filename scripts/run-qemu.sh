#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="${1:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
QEMU="${QEMU:-qemu-system-x86_64}"
ISO_PATH="$PROJECT_ROOT/build/fortify.iso"

command -v "$QEMU" >/dev/null 2>&1 || {
    echo "$QEMU is required to run the VM." >&2
    exit 1
}

if [[ ! -f "$ISO_PATH" ]]; then
    echo "Missing ISO: $ISO_PATH. Run scripts/create-image.sh or make image first." >&2
    exit 1
fi

"$QEMU" \
    -cdrom "$ISO_PATH" \
    -machine q35 \
    -cpu max \
    -m 1024 \
    -smp 4 \
    -serial stdio
