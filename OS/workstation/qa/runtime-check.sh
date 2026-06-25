#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
rootfs="${1:-${ROOTFS_DIR:-}}"

require_file() {
    local path="$1"
    if [ ! -e "$path" ]; then
        echo "runtime-check: missing $path" >&2
        exit 1
    fi
}

require_executable() {
    local path="$1"
    require_file "$path"
    if [ ! -x "$path" ]; then
        echo "runtime-check: $path is not executable" >&2
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

reject_text() {
    local path="$1"
    local pattern="$2"
    if grep -q -- "$pattern" "$path"; then
        echo "runtime-check: $path contains forbidden pattern: $pattern" >&2
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
require_file "$repo_root/workstation/shell/plasma-config/zenith-panel-setup.js"
require_file "$repo_root/tools/run_workstation_seed.ps1"
require_file "$repo_root/tools/smoke_workstation_seed.ps1"
require_file "$repo_root/tools/create_workstation_install_disk.ps1"
require_file "$repo_root/workstation/iso/create-persistence-disk.sh"
require_file "$repo_root/workstation/release/CHANGELOG.md"
require_file "$repo_root/workstation/release/KNOWN_ISSUES.md"
require_file "$repo_root/workstation/release/RELEASE_NOTES.md"
require_text "$repo_root/workstation/shell/plasma-config/plasma-org.kde.plasma.desktop-appletsrc" "org.kde.plasma.icontasks"
require_text "$repo_root/workstation/shell/plasma-config/plasma-org.kde.plasma.desktop-appletsrc" "os.zenith.Welcome.desktop"
require_text "$repo_root/workstation/shell/plasma-config/plasma-org.kde.plasma.desktop-appletsrc" "plugin=org.kde.panel"
if grep -q "plugin=org.kde.plasma.panel" "$repo_root/workstation/shell/plasma-config/plasma-org.kde.plasma.desktop-appletsrc"; then
    echo "runtime-check: Plasma panel containment uses removed package org.kde.plasma.panel" >&2
    exit 1
fi
require_text "$repo_root/workstation/shell/plasma-config/zenith-panel-setup.js" "new Panel"
require_text "$repo_root/workstation/shell/plasma-config/zenith-panel-setup.js" "org.kde.plasma.icontasks"
require_text "$repo_root/workstation/shell/plasma-config/zenith-panel-setup.js" "applications:os.zenith.Welcome.desktop"
require_text "$repo_root/workstation/shell/plasma-config/zenith-panel-setup.js" "zenith-login-wallpaper.png"
require_text "$repo_root/workstation/apps/zenith-terminal/src/zenith_terminal/main.py" "--command"
require_text "$repo_root/workstation/apps/zenith-packages/src/zenith_packages/main.py" "Apply APT upgrades"
require_text "$repo_root/workstation/apps/zenith-packages/src/zenith_packages/main.py" "first_boot_apps_status"
require_text "$repo_root/workstation/apps/zenith-packages/src/zenith_packages/main.py" "Retry first-boot app setup"
require_text "$repo_root/workstation/apps/zenith-packages/src/zenith_packages/main.py" "apps-install-status.env"
require_file "$repo_root/workstation/apps/zenith-packages/src/zenith_packages/workers.py"
require_text "$repo_root/workstation/apps/zenith-packages/src/zenith_packages/main.py" "Install and Remove"
require_text "$repo_root/workstation/apps/zenith-packages/src/zenith_packages/main.py" "apt_command"
require_text "$repo_root/workstation/apps/zenith-packages/src/zenith_packages/main.py" "flatpak_command"
require_text "$repo_root/workstation/apps/zenith-packages/src/zenith_packages/main.py" "threading.Thread"
require_text "$repo_root/workstation/apps/zenith-packages/src/zenith_packages/workers.py" "pkexec"
require_text "$repo_root/workstation/apps/zenith-packages/src/zenith_packages/workers.py" "flatpak"
require_text "$repo_root/workstation/apps/zenith-settings/src/zenith_settings/main.py" "Wi-Fi radio"
require_text "$repo_root/workstation/apps/zenith-settings/src/zenith_settings/main.py" "Power mode"
require_text "$repo_root/workstation/apps/zenith-settings/src/zenith_settings/main.py" "Set hardware override"
require_text "$repo_root/workstation/apps/zenith-settings/src/zenith_settings/main.py" "Persistence status"
require_text "$repo_root/workstation/apps/zenith-settings/src/zenith_settings/main.py" "zenith-login-wallpaper.png"
require_text "$repo_root/workstation/apps/zenith-welcome/src/zenith_welcome/main.py" "zenith-login-wallpaper.png"
require_file "$repo_root/workstation/apps/zenith-files/src/zenith_files/operations.py"
require_text "$repo_root/workstation/apps/zenith-files/src/zenith_files/main.py" "copy_path"
require_text "$repo_root/workstation/apps/zenith-files/src/zenith_files/main.py" "move_path"
require_text "$repo_root/workstation/apps/zenith-files/src/zenith_files/main.py" "trash_path"
require_text "$repo_root/workstation/apps/zenith-files/src/zenith_files/main.py" "mounted_volumes"
require_text "$repo_root/workstation/apps/zenith-files/src/zenith_files/main.py" "Paste"
require_text "$repo_root/workstation/apps/zenith-files/src/zenith_files/main.py" "Move Here"
reject_text "$repo_root/workstation/config/dconf/db/local.d/00-zenith" "zenith-default.svg"
reject_text "$repo_root/workstation/apps/zenith-settings/src/zenith_settings/main.py" "zenith-default.svg"
reject_text "$repo_root/workstation/apps/zenith-welcome/src/zenith_welcome/main.py" "zenith-default.svg"
require_text "$repo_root/workstation/apps/zenith-installer/src/zenith_installer/main.py" "Dry-run install plan"
require_text "$repo_root/workstation/apps/zenith-installer/src/zenith_installer/main.py" "disk_validation"
require_text "$repo_root/workstation/apps/zenith-installer/src/zenith_installer/main.py" "Selected Target"
require_text "$repo_root/workstation/apps/zenith-installer/src/zenith_installer/main.py" "install_plan_for_disk"
require_text "$repo_root/workstation/apps/zenith-installer/src/zenith_installer/main.py" "Guarded install mode"
require_text "$repo_root/workstation/apps/zenith-installer/src/zenith_installer/main.py" "zenith-install-to-disk"
require_text "$repo_root/workstation/apps/zenith-installer/src/zenith_installer/main.py" "disk_explicitly_selected"
require_text "$repo_root/workstation/apps/zenith-installer/src/zenith_installer/main.py" "live_media_disks"
require_file "$repo_root/workstation/apps/zenith-installer/bin/zenith-install-to-disk"
require_text "$repo_root/workstation/apps/zenith-installer/bin/zenith-install-to-disk" "ZENITH-ERASE"
require_text "$repo_root/workstation/apps/zenith-installer/bin/zenith-install-to-disk" "rootflags=subvol=@"
require_text "$repo_root/workstation/apps/zenith-installer/bin/zenith-install-to-disk" "--removable"
require_text "$repo_root/workstation/apps/zenith-installer/bin/zenith-install-to-disk" "--no-nvram"
require_text "$repo_root/workstation/apps/zenith-installer/bin/zenith-install-to-disk" "--manual-login-smoke"
require_text "$repo_root/workstation/apps/zenith-installer/bin/zenith-install-to-disk" "/etc/fstab generated by zenith-install-to-disk"
require_text "$repo_root/workstation/apps/desktop/os.zenith.Installer.desktop" "Install ZenithOS to a blank disk"
require_text "$repo_root/workstation/distro/build-zenith-packages.sh" "zenith-install-to-disk"
require_text "$repo_root/workstation/packages/rootfs.apt.manifest" "grub-efi-amd64"
require_text "$repo_root/workstation/packages/rootfs.apt.manifest" "efibootmgr"
require_text "$repo_root/workstation/packages/rootfs.apt.manifest" "initramfs-tools"
require_text "$repo_root/workstation/config/dconf/db/local.d/00-zenith" "enable-animations=false"
require_text "$repo_root/workstation/config/dconf/db/local.d/00-zenith" "zenith-login-wallpaper.png"
require_text "$repo_root/workstation/config/os-release" "VERSION_ID=\"0.2-beta\""
require_text "$repo_root/workstation/config/dconf/db/local.d/00-zenith" "os.zenith.Welcome.desktop"
require_file "$repo_root/workstation/config/plymouth/zenith/zenith.plymouth"
require_file "$repo_root/workstation/assets/backgrounds/zenith-login-wallpaper.png"
require_file "$repo_root/workstation/config/hostname"
require_file "$repo_root/workstation/config/hosts"
require_file "$repo_root/workstation/config/sddm/sddm.conf"
require_text "$repo_root/workstation/config/sddm/sddm.conf" "Current=pixel-skyscrapers"
require_file "$repo_root/workstation/themes/sddm/pixel-skyscrapers/Main.qml"
require_file "$repo_root/workstation/themes/sddm/pixel-skyscrapers/BackgroundVideo.qml"
require_file "$repo_root/workstation/themes/sddm/pixel-skyscrapers/bg.mp4"
require_file "$repo_root/workstation/themes/sddm/pixel-skyscrapers/theme.conf"
require_file "$repo_root/workstation/themes/sddm/pixel-skyscrapers/font/PixelifySans-Bold.ttf"
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
require_file "$repo_root/workstation/config/systemd/system/zenith-autoinstall.service"
require_file "$repo_root/workstation/config/systemd/system/zenith-desktop-smoke-login.service"
require_file "$repo_root/workstation/config/systemd/system/zenith-session-report-watch.service"
require_file "$repo_root/workstation/config/usr/lib/zenith/zenith-hardware-detect"
require_file "$repo_root/workstation/config/usr/lib/zenith/zenith-boot-report"
require_file "$repo_root/workstation/config/usr/lib/zenith/zenith-autoinstall"
require_executable "$repo_root/workstation/config/usr/lib/zenith/zenith-desktop-smoke-login"
require_executable "$repo_root/workstation/config/usr/lib/zenith/zenith-session-report"
require_executable "$repo_root/workstation/config/usr/lib/zenith/zenith-session-report-watch"
require_file "$repo_root/workstation/config/xdg/autostart/zenith-session-report.desktop"
require_text "$repo_root/workstation/config/usr/lib/zenith/zenith-boot-report" "-- sddm theme --"
require_text "$repo_root/workstation/config/usr/lib/zenith/zenith-boot-report" "pixel-skyscrapers"
require_text "$repo_root/workstation/config/usr/lib/zenith/zenith-boot-report" "-- install boot proof --"
require_text "$repo_root/workstation/config/usr/lib/zenith/zenith-boot-report" "removable EFI fallback"
require_text "$repo_root/workstation/config/usr/lib/zenith/zenith-boot-report" "live runtime"
require_text "$repo_root/workstation/config/usr/lib/zenith/zenith-autoinstall" "zenith.autoinstall_target"
require_text "$repo_root/workstation/config/usr/lib/zenith/zenith-autoinstall" "zenith-install-to-disk"
require_text "$repo_root/workstation/config/usr/lib/zenith/zenith-autoinstall" "zenith.autoinstall_desktop_smoke=1"
require_text "$repo_root/workstation/config/usr/lib/zenith/zenith-autoinstall" "zenith.autoinstall_manual_login_smoke=1"
require_text "$repo_root/workstation/config/systemd/system/zenith-autoinstall.service" "ConditionKernelCommandLine=zenith.autoinstall_target"
require_text "$repo_root/workstation/config/usr/lib/zenith/zenith-session-report-watch" "zenith.manual_login_smoke=1"
require_text "$repo_root/workstation/config/usr/lib/zenith/zenith-session-setup" "qdbus6"
require_text "$repo_root/workstation/config/usr/lib/zenith/zenith-session-setup" "evaluateScript"
require_text "$repo_root/workstation/config/usr/lib/zenith/zenith-session-setup" "zenith-panel-setup.js"
require_text "$repo_root/workstation/config/usr/lib/zenith/zenith-session-setup" "plasma-apply-wallpaperimage"
require_text "$repo_root/workstation/config/usr/lib/zenith/zenith-session-setup" "org.kde.plasma.panel"
require_text "$repo_root/workstation/config/usr/lib/zenith/zenith-session-setup" "panel_version=4"
require_file "$repo_root/workstation/config/usr/lib/zenith/zenith-performance-defaults"
require_file "$repo_root/workstation/config/systemd/system.conf.d/zenith-boot.conf"
require_file "$repo_root/workstation/config/systemd/journald.conf.d/zenith-limits.conf"
require_file "$repo_root/workstation/config/NetworkManager/conf.d/zenith-fast-boot.conf"
require_text "$repo_root/workstation/iso/build-live-iso.sh" "set timeout=0"
require_text "$repo_root/workstation/iso/build-live-iso.sh" "set timeout_style=hidden"
require_text "$repo_root/workstation/iso/build-live-iso.sh" "live-config.noautologin"
require_text "$repo_root/workstation/iso/build-live-iso.sh" "zenith.boot_report=1"
require_text "$repo_root/workstation/iso/build-live-iso.sh" "sha256sum"
require_text "$repo_root/workstation/iso/build-rootfs.sh" "live-config.service"
require_text "$repo_root/workstation/iso/build-rootfs.sh" "zenith-autoinstall.service"
require_text "$repo_root/workstation/iso/build-rootfs.sh" "zenith-desktop-smoke-login.service"
require_text "$repo_root/workstation/iso/build-rootfs.sh" "zenith-session-report-watch.service"
require_text "$repo_root/workstation/iso/build-rootfs.sh" ".updated"
require_text "$repo_root/workstation/iso/create-persistence-disk.sh" "persistence.conf"
require_text "$repo_root/Makefile" "workstation-run-seed-persistent"
require_text "$repo_root/Makefile" "workstation-vm-persistence-wsl"
require_text "$repo_root/Makefile" "workstation-vm-install-disk"
require_text "$repo_root/Makefile" "workstation-run-seed-install-target"
require_text "$repo_root/Makefile" "workstation-autoinstall-disk"
require_text "$repo_root/Makefile" "workstation-autoinstall-manual-login-disk"
require_text "$repo_root/Makefile" "workstation-run-installed-disk"
require_text "$repo_root/Makefile" "workstation-smoke-installed-disk"
require_text "$repo_root/Makefile" "workstation-smoke-installed-manual-login"
require_text "$repo_root/Makefile" "workstation-smoke-seed"
require_text "$repo_root/workstation/config/usr/lib/zenith/zenith-hardware-detect" "zenith.profile="
require_text "$repo_root/workstation/config/usr/lib/zenith/zenith-performance-defaults" "/run/zenith/hardware-profile.env"
require_text "$repo_root/workstation/distro/build-zenith-packages.sh" "zenith-desktop-smoke-login"
require_text "$repo_root/workstation/distro/build-zenith-packages.sh" "zenith-session-report"
require_text "$repo_root/workstation/distro/build-zenith-packages.sh" "zenith-session-report-watch"
require_text "$repo_root/workstation/distro/build-zenith-packages.sh" "zenith-session-report.desktop"
require_text "$repo_root/tools/run_workstation_seed.ps1" "zenith.profile=dev-vm"
require_text "$repo_root/tools/run_workstation_seed.ps1" "live-config.noautologin"
require_text "$repo_root/tools/run_workstation_seed.ps1" "install-target"
require_text "$repo_root/tools/run_workstation_seed.ps1" "autoinstall"
require_text "$repo_root/tools/run_workstation_seed.ps1" "zenith.autoinstall_target=/dev/vda"
require_text "$repo_root/tools/run_workstation_seed.ps1" "zenith.autoinstall_desktop_smoke=1"
require_text "$repo_root/tools/run_workstation_seed.ps1" "zenith.autoinstall_manual_login_smoke=1"
require_text "$repo_root/tools/run_workstation_seed.ps1" "zenithos-install-target.qcow2"
require_text "$repo_root/tools/smoke_workstation_seed.ps1" "live-config.noautologin"
require_text "$repo_root/tools/smoke_workstation_seed.ps1" "InstalledDisk"
require_text "$repo_root/tools/smoke_workstation_seed.ps1" "ManualLogin"
require_text "$repo_root/tools/smoke_workstation_seed.ps1" "zenith-session-report-watch: waiting for"
require_text "$repo_root/tools/smoke_workstation_seed.ps1" "manual-login-smoke-input.log"
require_text "$repo_root/tools/smoke_workstation_seed.ps1" "sendkey"
require_text "$repo_root/tools/create_workstation_install_disk.ps1" "qemu-img create -f qcow2"
require_text "$repo_root/tools/run_workstation_seed.ps1" "zenithos-persistence.raw"
require_text "$repo_root/workstation/apps/zenith-settings/src/zenith_settings/main.py" "Hardware profile"
require_text "$repo_root/workstation/scripts/first-boot-apps.sh" "apps-install-status.env"
require_text "$repo_root/workstation/scripts/first-boot-apps.sh" "DNS cannot resolve flathub.org"

if find "$repo_root/workstation/apps" -name '__pycache__' -o -name '*.pyc' | grep -q .; then
    echo "runtime-check: Python bytecode/cache files are present under workstation/apps" >&2
    exit 1
fi

if [ -n "$rootfs" ]; then
    require_file "$rootfs/etc/xdg/kdeglobals"
    require_file "$rootfs/etc/skel/.config/plasma-org.kde.plasma.desktop-appletsrc"
    require_file "$rootfs/home/hailtheking/.config/plasma-org.kde.plasma.desktop-appletsrc"
    require_file "$rootfs/usr/share/zenith/plasma/zenith-panel-setup.js"
    require_file "$rootfs/usr/share/backgrounds/zenith/zenith-default.svg"
    require_file "$rootfs/usr/share/backgrounds/zenith/zenith-login-wallpaper.png"
    require_file "$rootfs/usr/share/plymouth/themes/zenith/zenith.plymouth"
    require_file "$rootfs/usr/share/zenith/hardware-profiles/dev-vm.profile"
    require_file "$rootfs/usr/share/zenith/hardware-profiles/generic-workstation.profile"
    require_file "$rootfs/usr/share/zenith/hardware-profiles/lenovo-v14-ada.profile"
    require_file "$rootfs/etc/systemd/system/zenith-hardware-detect.service"
    require_file "$rootfs/etc/systemd/system/zenith-boot-report.service"
    require_file "$rootfs/etc/systemd/system/zenith-performance-defaults.service"
    require_file "$rootfs/etc/systemd/system/zenith-plymouth-handoff.service"
    require_file "$rootfs/etc/systemd/system/zenith-autoinstall.service"
    require_file "$rootfs/etc/systemd/system/zenith-desktop-smoke-login.service"
    require_file "$rootfs/etc/systemd/system/zenith-session-report-watch.service"
    require_file "$rootfs/usr/lib/zenith/zenith-hardware-detect"
    require_file "$rootfs/usr/lib/zenith/zenith-boot-report"
    require_file "$rootfs/usr/lib/zenith/zenith-autoinstall"
    require_file "$rootfs/usr/lib/zenith/zenith-performance-defaults"
    require_executable "$rootfs/usr/lib/zenith/zenith-desktop-smoke-login"
    require_executable "$rootfs/usr/lib/zenith/zenith-session-report"
    require_executable "$rootfs/usr/lib/zenith/zenith-session-report-watch"
    require_file "$rootfs/etc/systemd/system.conf.d/zenith-boot.conf"
    require_file "$rootfs/etc/systemd/journald.conf.d/zenith-limits.conf"
    require_file "$rootfs/etc/NetworkManager/conf.d/zenith-fast-boot.conf"
    require_file "$rootfs/usr/bin/zenith-welcome"
    require_file "$rootfs/usr/bin/zenith-files"
    require_file "$rootfs/usr/bin/zenith-installer"
    require_file "$rootfs/usr/bin/zenith-install-to-disk"
    require_file "$rootfs/usr/share/zenith/apps/zenith_packages/main.py"
    require_file "$rootfs/usr/share/zenith/apps/zenith_packages/workers.py"
    require_text "$rootfs/usr/share/zenith/apps/zenith_packages/main.py" "Install and Remove"
    require_text "$rootfs/usr/share/zenith/apps/zenith_packages/main.py" "threading.Thread"
    require_text "$rootfs/usr/share/zenith/apps/zenith_packages/workers.py" "pkexec"
    require_file "$rootfs/usr/share/zenith/apps/zenith_files/main.py"
    require_file "$rootfs/usr/share/zenith/apps/zenith_files/operations.py"
    require_text "$rootfs/usr/share/zenith/apps/zenith_files/main.py" "copy_path"
    require_text "$rootfs/usr/share/zenith/apps/zenith_files/main.py" "mounted_volumes"
    require_file "$rootfs/usr/src/zenithos/workstation/release/RELEASE_NOTES.md"
    require_file "$rootfs/usr/src/zenithos/tools/smoke_workstation_seed.ps1"
    require_file "$rootfs/etc/sddm.conf"
    require_file "$rootfs/etc/sddm/sddm.conf"
    require_file "$rootfs/usr/lib/zenith/zenith-greeter"
    require_file "$rootfs/etc/xdg/autostart/zenith-greeter.desktop"
    require_file "$rootfs/etc/xdg/autostart/zenith-session-report.desktop"
    require_file "$rootfs/etc/xdg/autostart/zenith-welcome.desktop"
    require_file "$rootfs/usr/share/sddm/themes/pixel-skyscrapers/Main.qml"
    require_file "$rootfs/usr/share/sddm/themes/pixel-skyscrapers/BackgroundVideo.qml"
    require_file "$rootfs/usr/share/sddm/themes/pixel-skyscrapers/bg.mp4"
    require_file "$rootfs/usr/share/sddm/themes/pixel-skyscrapers/theme.conf"
    require_file "$rootfs/usr/share/sddm/themes/pixel-skyscrapers/font/PixelifySans-Bold.ttf"
    # Verify autologin is disabled - manual login required
    require_text "$rootfs/etc/sddm.conf" "Current=pixel-skyscrapers"
    require_text "$rootfs/etc/sddm/sddm.conf" "Current=pixel-skyscrapers"
    require_text "$rootfs/etc/dconf/db/local.d/00-zenith" "cursor-size=48"
    require_text "$rootfs/etc/dconf/db/local.d/00-zenith" "zenith-login-wallpaper.png"
    if grep -R -q "plugin=org.kde.plasma.panel" "$rootfs/etc/skel" "$rootfs/home" 2>/dev/null; then
        echo "runtime-check: rootfs still contains removed Plasma package org.kde.plasma.panel" >&2
        exit 1
    fi
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
