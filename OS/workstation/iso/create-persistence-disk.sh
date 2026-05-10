#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
output="${1:-$repo_root/build/workstation/zenithos-persistence.raw}"
size="${ZENITH_PERSISTENCE_SIZE:-12G}"
label="${ZENITH_PERSISTENCE_LABEL:-persistence}"

require_command() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "missing required command: $1" >&2
        exit 1
    fi
}

usage() {
    cat <<EOF
Usage: $0 [output.raw]

Creates a sparse ext4 live-boot persistence disk.

Environment:
  ZENITH_PERSISTENCE_SIZE   Disk size, default: 12G
  ZENITH_PERSISTENCE_LABEL  Filesystem label, default: persistence
EOF
}

if [ "${1:-}" = "--help" ]; then
    usage
    exit 0
fi

require_command mkfs.ext4
require_command truncate
require_command mktemp

if [ -e "$output" ]; then
    echo "refusing to overwrite existing persistence disk: $output" >&2
    exit 1
fi

mkdir -p "$(dirname "$output")"
staging="$(mktemp -d)"
cleanup() {
    rm -rf "$staging"
}
trap cleanup EXIT

cat > "$staging/persistence.conf" <<'EOF'
/ union
EOF

truncate -s "$size" "$output"
mkfs.ext4 -F -L "$label" -d "$staging" "$output" >/dev/null

echo "ZenithOS persistence disk created: $output"
echo "Size: $size"
echo "Label: $label"
