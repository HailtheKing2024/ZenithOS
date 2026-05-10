#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
rootfs="${1:-}"
output="${2:-$repo_root/build/workstation/zenithos-workstation.iso}"
stage="${ZENITH_ISO_STAGE:-$repo_root/build/workstation/iso-stage}"

require_command() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "missing required command: $1" >&2
        exit 1
    fi
}

if [ -z "$rootfs" ]; then
    if [ -f "$repo_root/build/workstation/rootfs.path" ]; then
        rootfs="$(cat "$repo_root/build/workstation/rootfs.path")"
    elif [ -f "/var/lib/zenith-build/latest-rootfs.path" ]; then
        rootfs="$(cat /var/lib/zenith-build/latest-rootfs.path)"
    else
        echo "rootfs path required; run zenith-build rootfs first" >&2
        exit 1
    fi
fi

if [ ! -d "$rootfs" ]; then
    echo "rootfs not found: $rootfs" >&2
    exit 1
fi

require_command grub-mkrescue
require_command mformat
require_command mksquashfs
require_command xorriso

kernel="$(find "$rootfs/boot" -maxdepth 1 -type f -name 'vmlinuz-*' | sort | tail -n 1)"
initrd="$(find "$rootfs/boot" -maxdepth 1 -type f -name 'initrd.img-*' | sort | tail -n 1)"

if [ -z "$kernel" ] || [ -z "$initrd" ]; then
    echo "rootfs is missing /boot/vmlinuz-* or /boot/initrd.img-*" >&2
    exit 1
fi

rm -rf "$stage"
mkdir -p "$stage/boot/grub" "$stage/live" "$(dirname "$output")"

cp "$kernel" "$stage/live/vmlinuz"
cp "$initrd" "$stage/live/initrd.img"

sudo mksquashfs "$rootfs" "$stage/live/filesystem.squashfs" \
    -noappend \
    -comp zstd \
    -wildcards \
    -e 'var/lib/zenith-build/*' \
    -e 'tmp/*' \
    -e 'var/tmp/*'

cat > "$stage/boot/grub/grub.cfg" <<'EOF'
terminal_input console
terminal_output console

set timeout=1
set timeout_style=menu
set default=0
set gfxpayload=keep

set live_args="boot=live username=zenith hostname=zenithos locales=en_US.UTF-8 keyboard-layouts=us"
set serial_args="$live_args console=tty0 console=ttyS0,115200n8"
set debug_args="$serial_args systemd.show_status=1 loglevel=7 zenith.boot_report=1"

menuentry "ZenithOS Workstation" {
    linux /live/vmlinuz $live_args quiet splash loglevel=3 systemd.show_status=0 plymouth.ignore-serial-consoles
    initrd /live/initrd.img
}

menuentry "ZenithOS Workstation (verbose diagnostics)" {
    linux /live/vmlinuz $debug_args
    initrd /live/initrd.img
}

menuentry "ZenithOS Workstation (text rescue)" {
    linux /live/vmlinuz $debug_args systemd.unit=multi-user.target nomodeset
    initrd /live/initrd.img
}

menuentry "ZenithOS Workstation (live-boot shell)" {
    linux /live/vmlinuz $debug_args break=mount
    initrd /live/initrd.img
}
EOF

grub-mkrescue -o "$output" "$stage"

echo "ZenithOS live ISO created at $output"
