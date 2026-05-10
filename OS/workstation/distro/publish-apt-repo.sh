#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
package_dir="${ZENITH_PACKAGE_OUT:-$repo_root/build/workstation/packages}"
repo_dir="${ZENITH_APT_REPO:-$repo_root/build/workstation/apt-repo}"

require_command() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "missing required command: $1" >&2
        exit 1
    fi
}

require_command dpkg-scanpackages
require_command gzip

if ! compgen -G "$package_dir/*.deb" >/dev/null; then
    echo "no packages found in $package_dir" >&2
    exit 1
fi

rm -rf "$repo_dir"
mkdir -p "$repo_dir"
cp "$package_dir/"*.deb "$repo_dir/"

(
    cd "$repo_dir"
    dpkg-scanpackages . /dev/null > Packages
    gzip -9c Packages > Packages.gz
)

echo "Zenith apt repository published at $repo_dir"
