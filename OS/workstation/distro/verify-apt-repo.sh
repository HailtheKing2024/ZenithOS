#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
repo_dir="${ZENITH_APT_REPO:-$repo_root/build/workstation/apt-repo}"
manifest="$repo_root/workstation/distro/package-set.manifest"

if [ ! -f "$repo_dir/Packages" ]; then
    echo "missing apt Packages index: $repo_dir/Packages" >&2
    exit 1
fi

missing=0
while IFS= read -r package; do
    if ! grep -qx "Package: $package" "$repo_dir/Packages"; then
        echo "missing package from Zenith apt repo: $package" >&2
        missing=1
    fi
done < <(sed -e 's/#.*$//' -e '/^[[:space:]]*$/d' "$manifest")

if [ "$missing" -ne 0 ]; then
    exit 1
fi

echo "Zenith apt repo verification ok"
