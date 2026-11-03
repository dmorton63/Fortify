#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="${1:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
LIMINE_BINARIES="${2:-${LIMINE_BINARIES:-}}"
XORRISO="${XORRISO:-xorriso}"
LIMINE_DEPLOY="${LIMINE_DEPLOY:-limine-deploy}"

if [[ -z "$LIMINE_BINARIES" ]]; then
    echo "LIMINE_BINARIES is required." >&2
    echo "Usage: $0 [project_root] [limine_binaries]" >&2
    exit 1
fi

"$PROJECT_ROOT/scripts/build-kernel.sh" "$PROJECT_ROOT"

command -v "$XORRISO" >/dev/null 2>&1 || {
    echo "$XORRISO is required for ISO creation." >&2
    exit 1
}

ISO_ROOT="$PROJECT_ROOT/build/iso-root"
BOOT_DIR="$ISO_ROOT/boot"
EFI_BOOT_DIR="$ISO_ROOT/EFI/BOOT"
BUILD_DIR="$PROJECT_ROOT/build"
KERNEL_PATH="$BUILD_DIR/fortify.elf"
LIMINE_CONF="$PROJECT_ROOT/boot/limine.conf"
ISO_PATH="$BUILD_DIR/fortify.iso"

mkdir -p "$BOOT_DIR" "$EFI_BOOT_DIR"

cp "$KERNEL_PATH" "$BOOT_DIR/fortify.elf"
cp "$LIMINE_CONF" "$BOOT_DIR/limine.conf"
cp "$LIMINE_BINARIES/limine-bios.sys" "$BOOT_DIR/limine-bios.sys"
cp "$LIMINE_BINARIES/limine-bios-cd.bin" "$BOOT_DIR/limine-bios-cd.bin"
cp "$LIMINE_BINARIES/limine-uefi-cd.bin" "$BOOT_DIR/limine-uefi-cd.bin"
cp "$LIMINE_BINARIES/BOOTX64.EFI" "$EFI_BOOT_DIR/BOOTX64.EFI"
cp "$LIMINE_BINARIES/BOOTIA32.EFI" "$EFI_BOOT_DIR/BOOTIA32.EFI"

"$XORRISO" -as mkisofs \
    -b boot/limine-bios-cd.bin \
    -no-emul-boot \
    -boot-load-size 4 \
    -boot-info-table \
    --efi-boot boot/limine-uefi-cd.bin \
    -efi-boot-part \
    --efi-boot-image \
    --protective-msdos-label \
    "$ISO_ROOT" \
    -o "$ISO_PATH"

if command -v "$LIMINE_DEPLOY" >/dev/null 2>&1; then
    if "$LIMINE_DEPLOY" --help 2>/dev/null | grep -q "bios-install"; then
        "$LIMINE_DEPLOY" bios-install "$ISO_PATH"
    else
        "$LIMINE_DEPLOY" "$ISO_PATH"
    fi
elif [[ -x "$LIMINE_BINARIES/limine-deploy" ]]; then
    if "$LIMINE_BINARIES/limine-deploy" --help 2>/dev/null | grep -q "bios-install"; then
        "$LIMINE_BINARIES/limine-deploy" bios-install "$ISO_PATH"
    else
        "$LIMINE_BINARIES/limine-deploy" "$ISO_PATH"
    fi
else
    echo "Warning: limine-deploy not found in PATH or LIMINE_BINARIES. Skipping limine deploy step."
fi

echo "Built image: $ISO_PATH"
