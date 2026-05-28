#!/usr/bin/env bash
set -euo pipefail

repo="${ZENITH_SOURCE:-/usr/src/zenithos}"
output_root="${ZENITH_OUTPUT_ROOT:-/var/lib/zenith-build}"
timestamp="$(date +%Y%m%d-%H%M%S)"
command_name="${1:-help}"

usage() {
    cat <<'EOF'
zenith-build: self-hosted ZenithOS image builder

Commands:
  zenith-build check      Validate manifests, budget, and dependency graph.
  zenith-build rootfs     Build a new ZenithOS root filesystem.
  zenith-build iso        Build a live ISO from the latest rootfs.
  zenith-build clean      Remove generated self-hosted build outputs.

Environment:
  ZENITH_SOURCE           Source tree, default /usr/src/zenithos
  ZENITH_OUTPUT_ROOT      Build output root, default /var/lib/zenith-build
  ZENITH_SUITE            Debian suite for mmdebstrap, default trixie
  ZENITH_MIRROR           apt mirror, default http://deb.debian.org/debian
EOF
}

require_repo() {
    if [ ! -d "$repo" ]; then
        echo "ZenithOS source tree not found: $repo" >&2
        exit 1
    fi
}

require_command() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "missing required command: $1" >&2
        exit 1
    fi
}

run_check() {
    require_repo
    require_command python3
    python3 "$repo/tools/check_workstation.py"
}

run_rootfs() {
    require_repo
    require_command bash
    require_command mmdebstrap
    require_command rsync
    require_command sudo

    # Ensure signing key is available for the apt repo
    if [[ -z "$ZENITH_APT_SIGNING_KEY" ]]; then
        if [ -f "/etc/zenith/keys/build-signing-key.gpg" ]; then
            # Import secret key to ensure it's available in the current keyring
            gpg --batch --import-secret-keys "/etc/zenith/keys/build-signing-key.gpg" >/dev/null 2>&1 || true
            # Extract key ID from the imported key
            export ZENITH_APT_SIGNING_KEY="$(gpg --batch --list-secret-keys --with-colons "Zenith Build" | awk -F: '/^sec/ {print $5}' | head -n1)"
            if [[ -z "$ZENITH_APT_SIGNING_KEY" ]]; then
                export ZENITH_APT_SIGNING_KEY="/etc/zenith/keys/build-signing-key.gpg"
            fi
        else
            echo "No GPG signing key found. Generating a temporary build key..."
            sudo mkdir -p /etc/zenith/keys

            batch_file=$(mktemp)
            cat > "$batch_file" <<EOF
Key-Type: RSA
Key-Length: 2048
Name: Zenith Build
Passphrase:
%no-check-gpgfile-mode
%commit
EOF
            temp_gnupg=$(mktemp -d)
            GNUPGHOME="$temp_gnupg" gpg --batch --generate-key "$batch_file" >/dev/null 2>&1
            GNUPGHOME="$temp_gnupg" gpg --batch --export-secret-keys "Zenith Build" | sudo tee /etc/zenith/keys/build-signing-key.gpg > /dev/null

            export ZENITH_APT_SIGNING_KEY="$(GNUPGHOME="$temp_gnupg" gpg --batch --list-secret-keys --with-colons | awk -F: '/^sec/ {print $5}' | head -n1)"

            gpg --batch --import-secret-keys /etc/zenith/keys/build-signing-key.gpg >/dev/null 2>&1 || true

            rm -rf "$batch_file" "$temp_gnupg"
        fi
    fi

    local output="$output_root/rootfs-$timestamp"
    mkdir -p "$output_root"
    run_check
    bash "$repo/workstation/iso/build-rootfs.sh" "$output"
    echo "$output" > "$output_root/latest-rootfs.path"
    echo "self-hosted rootfs: $output"

}

run_iso() {
    require_repo
    require_command bash
    require_command grub-mkrescue
    require_command mksquashfs
    require_command xorriso

    local rootfs_path=""
    local output="$output_root/zenithos-workstation-$timestamp.iso"

    if [ -f "$output_root/latest-rootfs.path" ]; then
        rootfs_path="$(cat "$output_root/latest-rootfs.path")"
    fi

    if [ -z "$rootfs_path" ]; then
        echo "No latest rootfs recorded. Run zenith-build rootfs first." >&2
        exit 1
    fi

    run_check
    bash "$repo/workstation/iso/build-live-iso.sh" "$rootfs_path" "$output"
    echo "$output" > "$output_root/latest-iso.path"
    echo "self-hosted ISO: $output"
}

run_clean() {
    if [ "$output_root" = "/" ] || [ -z "$output_root" ]; then
        echo "refusing unsafe output root: $output_root" >&2
        exit 1
    fi

    rm -rf "$output_root"
    echo "removed $output_root"
}

case "$command_name" in
    check)
        run_check
        ;;
    rootfs)
        run_rootfs
        ;;
    iso)
        run_iso
        ;;
    clean)
        run_clean
        ;;
    help|-h|--help)
        usage
        ;;
    *)
        usage >&2
        exit 1
        ;;
esac
