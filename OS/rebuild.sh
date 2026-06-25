#!/bin/bash
# ZenithOS workstation rebuild — convenience wrapper.
#
# The Makefile already exposes `workstation-rootfs` and `workstation-seed-iso`.
# This script simply runs them in sequence with the right signing-key env var.
#
# Run from this directory:
#   ./rebuild.sh                # build packages + rootfs + iso
#   ./rebuild.sh --no-packages  # skip rebuild of zenith-*.debs
#
# For finer control, use the Makefile directly:
#   make workstation-check         # validate file layout
#   bash workstation/qa/runtime-check.sh
#   make workstation-packages
#   make workstation-rootfs
#   make workstation-seed-iso

set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$repo_root"

export ZENITH_APT_SIGNING_KEY="${ZENITH_APT_SIGNING_KEY:-}"

if [ "${1:-}" != "--no-packages" ]; then
    make workstation-packages
fi

make workstation-rootfs
make workstation-seed-iso

echo
echo "ISO ready:"
ls -lh build/workstation/zenithos-seed.iso 2>/dev/null || true
sha256sum -b build/workstation/zenithos-seed.iso* 2>/dev/null || true
