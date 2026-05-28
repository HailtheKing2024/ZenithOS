#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
default_output="${ZENITH_BUILD_ROOT:-/var/lib/zenith-build/rootfs}"

output="${1:-$default_output}"
suite="${ZENITH_SUITE:-trixie}"
mirror="${ZENITH_MIRROR:-http://deb.debian.org/debian}"
components="${ZENITH_COMPONENTS:-main,contrib,non-free,non-free-firmware}"
zenith_repo="${ZENITH_APT_REPO:-$repo_root/build/workstation/apt-repo}"

manifest_items() {
    local file="$1"
    sed -e 's/#.*$//' -e '/^[[:space:]]*$/d' "$file" | tr '\n' ',' | sed 's/,$//'
}

require_command() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "missing required command: $1" >&2
        exit 1
    fi
}

require_command mmdebstrap
require_command rsync
require_command sudo

bash "$repo_root/workstation/distro/build-zenith-packages.sh"
bash "$repo_root/workstation/distro/publish-apt-repo.sh"

if [ -e "$output" ]; then
    echo "existing rootfs found at $output; removing for clean build"
    sudo rm -rf "$output"
fi

base_manifest="$repo_root/workstation/packages/rootfs.apt.manifest"
include_packages="$(manifest_items "$base_manifest")"

mkdir -p "$(dirname "$output")"

sudo mmdebstrap \
    --variant=apt \
    --components="$components" \
    --aptopt='Acquire::Languages "none"' \
    --include="$include_packages" \
    "$suite" \
    "$output" \
    "$mirror"

sudo mkdir -p "$output/var/lib/zenith-repo" "$output/etc/apt/sources.list.d"
sudo rsync -a --delete "$zenith_repo/" "$output/var/lib/zenith-repo/"
zenith_source="$(mktemp)"
cat > "$zenith_source" <<'EOF'
deb [signed-by=/var/lib/zenith-repo/zenith-archive-keyring.gpg] file:/var/lib/zenith-repo ./
EOF
sudo install -m 0644 "$zenith_source" "$output/etc/apt/sources.list.d/zenith-local.list"
rm -f "$zenith_source"

sudo chroot "$output" apt-get update
zenith_packages="$(sed -e 's/#.*$//' -e '/^[[:space:]]*$/d' "$repo_root/workstation/distro/package-set.manifest" | tr '\n' ' ')"
sudo chroot "$output" apt-get install -y $zenith_packages

sudo mkdir -p "$output/usr/src/zenithos"
sudo rsync -a --delete \
    --exclude-from "$repo_root/workstation/selfhost/sources.exclude" \
    "$repo_root/" \
    "$output/usr/src/zenithos/"

# Create hailtheking user with password authentication
if ! sudo chroot "$output" getent passwd hailtheking >/dev/null; then
 sudo chroot "$output" useradd -m -s /bin/bash hailtheking
fi
printf 'hailtheking:2976880801\n' | sudo chroot "$output" chpasswd

# Add hailtheking to required groups
for group in sudo adm audio video plugdev netdev; do
 if sudo chroot "$output" getent group "$group" >/dev/null; then
 sudo chroot "$output" usermod -aG "$group" hailtheking
 fi
done

sudo chroot "$output" dconf update
if sudo chroot "$output" sh -c 'command -v plymouth-set-default-theme >/dev/null 2>&1'; then
    sudo chroot "$output" plymouth-set-default-theme zenith >/dev/null 2>&1 || true
    sudo chroot "$output" update-initramfs -u >/dev/null 2>&1 || true
fi
# Display manager: SDDM (with ZenithOS theme) - GDM removed
# gdm3 is installed as a dependency but should be disabled
sudo chroot "$output" systemctl disable gdm.service >/dev/null 2>&1 || true
sudo chroot "$output" systemctl mask gdm.service >/dev/null 2>&1 || true
sudo chroot "$output" systemctl enable sddm.service >/dev/null
sudo chroot "$output" systemctl enable NetworkManager.service >/dev/null
sudo chroot "$output" systemctl enable zenith-firstboot.service >/dev/null
sudo chroot "$output" systemctl enable zenith-hardware-detect.service >/dev/null
sudo chroot "$output" systemctl enable zenith-boot-report.service >/dev/null
sudo chroot "$output" systemctl enable zenith-performance-defaults.service >/dev/null
sudo chroot "$output" systemctl enable zenith-plymouth-handoff.service >/dev/null
sudo chroot "$output" systemctl enable zenith-first-boot-apps.service >/dev/null
for unit in \
    apt-daily.timer \
    apt-daily-upgrade.timer \
    man-db.timer \
    e2scrub_all.timer \
    NetworkManager-wait-online.service \
    systemd-networkd-wait-online.service; do
    sudo chroot "$output" systemctl disable "$unit" >/dev/null 2>&1 || true
done
for unit in NetworkManager-wait-online.service systemd-networkd-wait-online.service; do
    sudo chroot "$output" systemctl mask "$unit" >/dev/null 2>&1 || true
done
sudo chroot "$output" systemctl mask live-config.service >/dev/null 2>&1 || true
sudo rm -f \
    "$output/etc/systemd/system/network-online.target.wants/NetworkManager-wait-online.service" \
    "$output/etc/systemd/system/network-online.target.wants/systemd-networkd-wait-online.service"
sudo touch "$output/etc/.updated" "$output/var/.updated"

mkdir -p "$repo_root/build/workstation"
cat > "$repo_root/build/workstation/rootfs.path" <<EOF
$output
EOF

echo "ZenithOS rootfs created at $output"
echo "Rootfs path recorded at $repo_root/build/workstation/rootfs.path"
echo "Next: enter a chroot or systemd-nspawn environment to run dconf update and package validation."
