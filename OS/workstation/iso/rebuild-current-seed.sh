#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
rootfs_path_file="$repo_root/build/workstation/rootfs.path"
output="${1:-$repo_root/build/workstation/zenithos-seed.iso}"

if [ ! -f "$rootfs_path_file" ]; then
    echo "rootfs path not recorded: $rootfs_path_file" >&2
    exit 1
fi

rootfs="$(cat "$rootfs_path_file")"
if [ ! -d "$rootfs" ]; then
    echo "rootfs not found: $rootfs" >&2
    exit 1
fi

bash "$repo_root/workstation/distro/build-zenith-packages.sh"
bash "$repo_root/workstation/distro/publish-apt-repo.sh"

mkdir -p "$rootfs/etc/systemd/system" "$rootfs/usr/src/zenithos"
ln -sfn /dev/null "$rootfs/etc/systemd/system/live-config.service"

mkdir -p "$rootfs/var/lib/zenith-repo" "$rootfs/etc/apt/sources.list.d"
rsync -a --delete "$repo_root/build/workstation/apt-repo/" "$rootfs/var/lib/zenith-repo/"
cat > "$rootfs/etc/apt/sources.list.d/zenith-local.list" <<'EOF'
deb [trusted=yes] file:/var/lib/zenith-repo ./
EOF
chroot "$rootfs" apt-get update
chroot "$rootfs" env DEBIAN_FRONTEND=noninteractive dpkg --configure -a || true
chroot "$rootfs" env DEBIAN_FRONTEND=noninteractive apt-get -f install -y || true
chroot "$rootfs" env DEBIAN_FRONTEND=noninteractive apt-get install -y --reinstall \
    zenith-settings \
    zenith-terminal \
    zenith-packages \
    zenith-files \
    zenith-welcome \
    zenith-installer \
    zenith-shell-extension \
    zenith-workstation-defaults \
    zenith-builder

chroot "$rootfs" systemctl enable zenith-hardware-detect.service >/dev/null 2>&1 || true

if chroot "$rootfs" getent passwd zenith >/dev/null; then
    printf 'zenith:zenith\n' | chroot "$rootfs" chpasswd
fi
rm -f "$rootfs/home/zenith/.config/zenith/greeter-shown"

rsync -a --delete \
    --exclude-from "$repo_root/workstation/selfhost/sources.exclude" \
    "$repo_root/" \
    "$rootfs/usr/src/zenithos/"

touch "$rootfs/etc/.updated" "$rootfs/var/.updated"

bash "$repo_root/workstation/iso/build-live-iso.sh" "$rootfs" "$output"

echo "Current seed ISO rebuilt from $rootfs"
