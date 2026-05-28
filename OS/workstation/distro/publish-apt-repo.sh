#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
package_dir="${ZENITH_PACKAGE_OUT:-$repo_root/build/workstation/packages}"
repo_dir="${ZENITH_APT_REPO:-$repo_root/build/workstation/apt-repo}"
signing_key="${ZENITH_APT_SIGNING_KEY:-}"

require_command() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "missing required command: $1" >&2
        exit 1
    fi
}

require_command dpkg-scanpackages
require_command gzip
require_command gpg
require_command md5sum
require_command sha256sum
require_command stat

if [[ -z "$signing_key" ]]; then
    echo "ZENITH_APT_SIGNING_KEY must name a local GPG signing key" >&2
    exit 1
fi

if ! gpg --batch --list-secret-keys "$signing_key" >/dev/null 2>&1; then
    echo "missing GPG secret key for Zenith apt repo: $signing_key" >&2
    exit 1
fi

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
    {
        echo "Origin: ZenithOS"
        echo "Label: ZenithOS"
        echo "Suite: stable"
        echo "Codename: zenith"
        echo "Architectures: all amd64"
        echo "Components: main"
        echo "Description: ZenithOS local package repository"
        echo "Date: $(date -Ru)"
        echo "MD5Sum:"
        for file in Packages Packages.gz; do
            printf " %s %s %s\n" "$(md5sum "$file" | awk '{print $1}')" "$(stat -c '%s' "$file")" "$file"
        done
        echo "SHA256:"
        for file in Packages Packages.gz; do
            printf " %s %s %s\n" "$(sha256sum "$file" | awk '{print $1}')" "$(stat -c '%s' "$file")" "$file"
        done
    } > Release
    gpg --batch --yes --local-user "$signing_key" --clearsign --digest-algo SHA256 -o InRelease Release
    gpg --batch --yes --local-user "$signing_key" --armor --detach-sign -o Release.gpg Release
    gpg --batch --yes --export "$signing_key" > zenith-archive-keyring.gpg
)

echo "Zenith apt repository published at $repo_dir"
