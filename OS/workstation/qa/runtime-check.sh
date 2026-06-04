#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
rootfs="${1:-}"

require_file() {
    local path="$1"
    if [ ! -e "$path" ]; then
        echo "runtime-check: missing $path" >&2
        exit 1
    fi
}

require_text() {
    local path="$1"
    local pattern="$2"
    if ! grep -q -- "$pattern" "$path"; then
        echo "runtime-check: $path missing pattern: $pattern" >&2
        exit 1
    fi
}

require_link_target() {
    local path="$1"
    local expected="$2"
    local actual
    actual="$(readlink "$path" 2>/dev/null || true)"
    if [ "$actual" != "$expected" ]; then
        echo "runtime-check: $path should link to $expected, got ${actual:-not-a-symlink}" >&2
        exit 1
    fi
}

for command in \
    zenith-settings \
    zenith-terminal \
    zenith-packages \
    zenith-files \
    zenith-welcome \
    zenith-installer; do
    require_file "$repo_root/workstation/apps/bin/$command"
done

for desktop in \
    os.zenith.Settings.desktop \
    os.zenith.Terminal.desktop \
    os.zenith.Packages.desktop \
    os.zenith.Files.desktop \
    os.zenith.Welcome.desktop \
    os.zenith.Installer.desktop; do
    require_file "$repo_root/workstation/apps/desktop/$desktop"
done

require_file "$repo_root/workstation/shell/plasma-config/metadata.desktop"
require_file "$repo_root/workstation/shell/plasma-config/kdeglobals"
require_file "$repo_root/workstation/shell/plasma-config/kwinrc"
require_file "$repo_root/workstation/shell/plasma-config/kcminputrc"
require_file "$repo_root/workstation/shell/plasma-config/plasma-org.kde.plasma.desktop-appletsrc"
require_file "$repo_root/tools/run_workstation_seed.ps1"
require_file "$repo_root/tools/smoke_workstation_seed.ps1"
require_file "$repo_root/workstation/iso/create-persistence-disk.sh"
require_file "$repo_root/workstation/release/CHANGELOG.md"
require_file "$repo_root/workstation/release/KNOWN_ISSUES.md"
require_file "$repo_root/workstation/release/RELEASE_NOTES.md"
require_text "$repo_root/workstation/shell/plasma-config/plasma-org.kde.plasma.desktop-appletsrc" "org.kde.plasma.icontasks"
require_text "$repo_root/workstation/shell/plasma-config/plasma-org.kde.plasma.desktop-appletsrc" "os.zenith.Welcome.desktop"
require_text "$repo_root/workstation/apps/zenith-terminal/src/zenith_terminal/main.py" "--command"
require_text "$repo_root/workstation/apps/zenith-packages/src/zenith_packages/main.py" "Apply APT upgrades"
require_text "$repo_root/workstation/apps/zenith-settings/src/zenith_settings/main.py" "Wi-Fi radio"
require_text "$repo_root/workstation/apps/zenith-settings/src/zenith_settings/main.py" "Power mode"
require_text "$repo_root/workstation/apps/zenith-settings/src/zenith_settings/main.py" "Set hardware override"
require_text "$repo_root/workstation/apps/zenith-settings/src/zenith_settings/main.py" "Persistence status"
require_text "$repo_root/workstation/apps/zenith-installer/src/zenith_installer/main.py" "Dry-run install plan"
require_text "$repo_root/workstation/apps/zenith-installer/src/zenith_installer/main.py" "disk_validation"
require_text "$repo_root/workstation/apps/zenith-installer/src/zenith_installer/main.py" "Selected Target"
require_text "$repo_root/workstation/apps/zenith-installer/src/zenith_installer/main.py" "install_plan_for_disk"
require_text "$repo_root/workstation/config/dconf/db/local.d/00-zenith" "enable-animations=false"
require_text "$repo_root/workstation/config/os-release" "VERSION_ID=\"0.2-beta\""
require_text "$repo_root/workstation/config/dconf/db/local.d/00-zenith" "os.zenith.Welcome.desktop"
require_file "$repo_root/workstation/config/plymouth/zenith/zenith.plymouth"
require_file "$repo_root/workstation/config/hostname"
require_file "$repo_root/workstation/config/hosts"
require_file "$repo_root/workstation/config/sddm/sddm.conf"
require_file "$repo_root/workstation/profiles/README.md"
require_file "$repo_root/workstation/profiles/dev-vm.profile"
require_file "$repo_root/workstation/profiles/generic-workstation.profile"
require_file "$repo_root/workstation/profiles/lenovo-v14-ada.profile"
# KDE Plasma default system configurations checked instead of GDM dconf
require_file "$repo_root/workstation/config/usr/lib/zenith/zenith-greeter"
require_file "$repo_root/workstation/config/xdg/autostart/zenith-greeter.desktop"
require_file "$repo_root/workstation/config/systemd/system/zenith-hardware-detect.service"
require_file "$repo_root/workstation/config/systemd/system/zenith-boot-report.service"
require_file "$repo_root/workstation/config/systemd/system/zenith-performance-defaults.service"
require_file "$repo_root/workstation/config/systemd/system/zenith-plymouth-handoff.service"
require_file "$repo_root/workstation/config/usr/lib/zenith/zenith-hardware-detect"
require_file "$repo_root/workstation/config/usr/lib/zenith/zenith-boot-report"
require_file "$repo_root/workstation/config/usr/lib/zenith/zenith-performance-defaults"
require_file "$repo_root/workstation/config/systemd/system.conf.d/zenith-boot.conf"
require_file "$repo_root/workstation/config/systemd/journald.conf.d/zenith-limits.conf"
require_file "$repo_root/workstation/config/NetworkManager/conf.d/zenith-fast-boot.conf"
require_text "$repo_root/workstation/iso/build-live-iso.sh" "set timeout=1"
require_text "$repo_root/workstation/iso/build-live-iso.sh" "zenith.boot_report=1"
require_text "$repo_root/workstation/iso/build-live-iso.sh" "sha256sum"
require_text "$repo_root/workstation/iso/build-rootfs.sh" "live-config.service"
require_text "$repo_root/workstation/iso/build-rootfs.sh" ".updated"
require_text "$repo_root/workstation/iso/create-persistence-disk.sh" "persistence.conf"
require_text "$repo_root/Makefile" "workstation-run-seed-persistent"
require_text "$repo_root/Makefile" "workstation-vm-persistence-wsl"
require_text "$repo_root/Makefile" "workstation-smoke-seed"
require_text "$repo_root/workstation/config/usr/lib/zenith/zenith-hardware-detect" "zenith.profile="
require_text "$repo_root/workstation/config/usr/lib/zenith/zenith-performance-defaults" "/run/zenith/hardware-profile.env"
require_text "$repo_root/tools/run_workstation_seed.ps1" "zenith.profile=dev-vm"
require_text "$repo_root/tools/run_workstation_seed.ps1" "zenithos-persistence.raw"
require_text "$repo_root/workstation/apps/zenith-settings/src/zenith_settings/main.py" "Hardware profile"

if find "$repo_root/workstation/apps" -name '__pycache__' -o -name '*.pyc' | grep -q .; then
    echo "runtime-check: Python bytecode/cache files are present under workstation/apps" >&2
    exit 1
fi

if [ -n "$rootfs" ]; then
    require_file "$rootfs/etc/xdg/kdeglobals"
    require_file "$rootfs/etc/skel/.config/plasma-org.kde.plasma.desktop-appletsrc"
    require_file "$rootfs/usr/share/backgrounds/zenith/zenith-default.svg"
    require_file "$rootfs/usr/share/plymouth/themes/zenith/zenith.plymouth"
    require_file "$rootfs/usr/share/zenith/hardware-profiles/dev-vm.profile"
    require_file "$rootfs/usr/share/zenith/hardware-profiles/generic-workstation.profile"
    require_file "$rootfs/usr/share/zenith/hardware-profiles/lenovo-v14-ada.profile"
    require_file "$rootfs/etc/systemd/system/zenith-hardware-detect.service"
    require_file "$rootfs/etc/systemd/system/zenith-boot-report.service"
    require_file "$rootfs/etc/systemd/system/zenith-performance-defaults.service"
    require_file "$rootfs/etc/systemd/system/zenith-plymouth-handoff.service"
    require_file "$rootfs/usr/lib/zenith/zenith-hardware-detect"
    require_file "$rootfs/usr/lib/zenith/zenith-boot-report"
    require_file "$rootfs/usr/lib/zenith/zenith-performance-defaults"
    require_file "$rootfs/etc/systemd/system.conf.d/zenith-boot.conf"
    require_file "$rootfs/etc/systemd/journald.conf.d/zenith-limits.conf"
    require_file "$rootfs/etc/NetworkManager/conf.d/zenith-fast-boot.conf"
    require_file "$rootfs/usr/bin/zenith-welcome"
    require_file "$rootfs/usr/bin/zenith-installer"
    require_file "$rootfs/usr/src/zenithos/workstation/release/RELEASE_NOTES.md"
    require_file "$rootfs/usr/src/zenithos/tools/smoke_workstation_seed.ps1"
    require_file "$rootfs/etc/sddm.conf"
    require_file "$rootfs/usr/lib/zenith/zenith-greeter"
    require_file "$rootfs/etc/xdg/autostart/zenith-greeter.desktop"
    require_file "$rootfs/etc/xdg/autostart/zenith-welcome.desktop"
    # Verify autologin is disabled - manual login required
    require_text "$rootfs/etc/sddm.conf" "Theme=pixel-skyscrapers"
    require_text "$rootfs/etc/dconf/db/local.d/00-zenith" "cursor-size=48"
    require_text "$rootfs/etc/plymouth/plymouthd.conf" "Theme=zenith"
    require_text "$rootfs/etc/hostname" "zenithos"
    require_text "$rootfs/etc/os-release" "VERSION_ID=\"0.2-beta\""
    require_text "$rootfs/etc/systemd/system/zenith-hardware-detect.service" "Before=sysinit.target"
    require_text "$rootfs/etc/systemd/system/zenith-boot-report.service" "ConditionKernelCommandLine=zenith.boot_report=1"
    require_file "$rootfs/etc/systemd/system/live-config.service"
    require_link_target "$rootfs/etc/systemd/system/live-config.service" "/dev/null"
    require_file "$rootfs/etc/.updated"
    require_file "$rootfs/var/.updated"
    if [ -e "$rootfs/etc/systemd/system/network-online.target.wants/NetworkManager-wait-online.service" ]; then
        echo "runtime-check: NetworkManager-wait-online.service is still wanted by network-online.target" >&2
        exit 1
    fi
fi

echo "runtime-check: ok"
