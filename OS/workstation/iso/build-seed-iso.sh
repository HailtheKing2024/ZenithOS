#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
output="${1:-$repo_root/build/workstation/zenithos-seed.iso}"

require_command() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "missing required command: $1" >&2
        exit 1
    fi
}

require_command python3
require_command bash
require_command mmdebstrap
require_command rsync
require_command sudo
require_command grub-mkrescue
require_command mksquashfs
require_command xorriso

python3 "$repo_root/tools/check_workstation.py"

seed_rootfs="${ZENITH_SEED_ROOTFS:-$HOME/zenithos-build/rootfs-seed-$(date +%Y%m%d-%H%M%S)}"
bash "$repo_root/workstation/iso/build-rootfs.sh" "$seed_rootfs"
rootfs="$(cat "$repo_root/build/workstation/rootfs.path")"

bash "$repo_root/workstation/iso/build-live-iso.sh" "$rootfs" "$output"

echo "Seed ISO ready: $output"
echo "Boot this ISO once; future builds should use zenith-build from inside ZenithOS."
