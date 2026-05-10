#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
suite="${ZENITH_SUITE:-trixie}"
mirror="${ZENITH_MIRROR:-http://deb.debian.org/debian}"
components="${ZENITH_COMPONENTS:-main contrib non-free non-free-firmware}"
workdir="${ZENITH_APT_CHECK_DIR:-$HOME/.cache/zenithos/apt-check-$suite}"
manifest="$repo_root/workstation/packages/rootfs.apt.manifest"

require_command() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "missing required command: $1" >&2
        exit 1
    fi
}

require_command apt-get
require_command apt-cache

mkdir -p \
    "$workdir/etc/apt" \
    "$workdir/etc/apt/sources.list.d" \
    "$workdir/lists/partial" \
    "$workdir/var/lib/dpkg" \
    "$workdir/cache" \
    "$workdir/archives/partial"
touch "$workdir/var/lib/dpkg/status"

{
    for component in $components; do
        echo "deb [signed-by=/usr/share/keyrings/debian-archive-keyring.gpg] $mirror $suite $component"
    done
} > "$workdir/etc/apt/sources.list"

apt_opts=(
    -o "Dir::Etc::sourcelist=$workdir/etc/apt/sources.list"
    -o "Dir::Etc::sourceparts=$workdir/etc/apt/sources.list.d"
    -o "Dir::State::lists=$workdir/lists"
    -o "Dir::State::status=$workdir/var/lib/dpkg/status"
    -o "Dir::Cache=$workdir/cache"
    -o "Dir::Cache::archives=$workdir/archives"
    -o "Dir::Cache::pkgcache=$workdir/cache/pkgcache.bin"
    -o "Dir::Cache::srcpkgcache=$workdir/cache/srcpkgcache.bin"
    -o "Acquire::Languages=none"
)

apt-get "${apt_opts[@]}" update

missing=0
while IFS= read -r package; do
    candidate="$(apt-cache "${apt_opts[@]}" policy "$package" | awk '/Candidate:/ {candidate=$2} END {if (candidate != "") print candidate}')"
    if [ -z "$candidate" ] || [ "$candidate" = "(none)" ]; then
        echo "no install candidate in $suite: $package" >&2
        missing=1
    fi
done < <(sed -e 's/#.*$//' -e '/^[[:space:]]*$/d' "$manifest")

if [ "$missing" -ne 0 ]; then
    exit 1
fi

echo "apt package check ok for $suite"
