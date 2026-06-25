#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
out_dir="${ZENITH_PACKAGE_OUT:-$repo_root/build/workstation/packages}"
default_work_dir="/var/lib/zenith-build/package-work"
work_dir="${ZENITH_PACKAGE_WORK:-$default_work_dir}"
version="${ZENITH_VERSION:-0.1.0}"

require_command() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "missing required command: $1" >&2
        exit 1
    fi
}

pkg_root() {
    local name="$1"
    echo "$work_dir/$name"
}

write_control() {
    local name="$1"
    local description="$2"
    local depends="$3"
    local root
    root="$(pkg_root "$name")"
    mkdir -p "$root/DEBIAN"
    cat > "$root/DEBIAN/control" <<EOF
Package: $name
Version: $version
Section: x11
Priority: optional
Architecture: all
Maintainer: ZenithOS Team <maintainers@zenithos.local>
Depends: $depends
Description: $description
EOF
}

install_app_package() {
    local package="$1"
    local module="$2"
    local command="$3"
    local desktop="$4"
    local description="$5"
    local depends="$6"
    local root
    root="$(pkg_root "$package")"

    write_control "$package" "$description" "$depends"
    mkdir -p "$root/usr/share/zenith/apps" "$root/usr/bin" "$root/usr/share/applications"
    cp -R "$repo_root/workstation/apps/$package/src/$module" "$root/usr/share/zenith/apps/$module"
    install -m 0755 "$repo_root/workstation/apps/bin/$command" "$root/usr/bin/$command"
    install -m 0644 "$repo_root/workstation/apps/desktop/$desktop" "$root/usr/share/applications/$desktop"
}

build_package() {
    local name="$1"
    local root
    root="$(pkg_root "$name")"
    dpkg-deb --root-owner-group --build "$root" "$out_dir/${name}_${version}_all.deb" >/dev/null
    echo "built $out_dir/${name}_${version}_all.deb"
}

require_command dpkg-deb

rm -rf "$work_dir"
mkdir -p "$out_dir" "$work_dir"
rm -f "$out_dir"/*.deb

install_app_package \
    zenith-settings \
    zenith_settings \
    zenith-settings \
    os.zenith.Settings.desktop \
    "ZenithOS settings application" \
    "python3, python3-gi, gir1.2-gtk-4.0, gir1.2-adw-1"

install_app_package \
    zenith-terminal \
    zenith_terminal \
    zenith-terminal \
    os.zenith.Terminal.desktop \
    "ZenithOS terminal application" \
    "python3, python3-gi, gir1.2-gtk-4.0, gir1.2-adw-1, gir1.2-vte-3.91"

install_app_package \
    zenith-packages \
    zenith_packages \
    zenith-packages \
    os.zenith.Packages.desktop \
    "ZenithOS package manager" \
    "python3, python3-gi, gir1.2-gtk-4.0, gir1.2-adw-1, apt, flatpak"
install -m 0755 "$repo_root/workstation/apps/bin/zpkg" "$(pkg_root zenith-packages)/usr/bin/zpkg"

install_app_package \
    zenith-files \
    zenith_files \
    zenith-files \
    os.zenith.Files.desktop \
    "ZenithOS file manager" \
    "python3, python3-gi, gir1.2-gtk-4.0, gir1.2-adw-1"

install_app_package \
    zenith-welcome \
    zenith_welcome \
    zenith-welcome \
    os.zenith.Welcome.desktop \
    "ZenithOS welcome and first-login status application" \
    "python3, python3-gi, gir1.2-gtk-4.0, gir1.2-adw-1"

install_app_package \
    zenith-installer \
    zenith_installer \
    zenith-installer \
    os.zenith.Installer.desktop \
    "ZenithOS guarded disk installer" \
    "python3, python3-gi, gir1.2-gtk-4.0, gir1.2-adw-1, sudo, util-linux, parted, gdisk, btrfs-progs, dosfstools, rsync, grub-common, grub2-common, grub-efi-amd64, grub-efi-amd64-bin, efibootmgr, initramfs-tools"
install -D -m 0755 "$repo_root/workstation/apps/zenith-installer/bin/zenith-install-to-disk" \
    "$(pkg_root zenith-installer)/usr/bin/zenith-install-to-disk"

install_app_package \
    zenith-calculator \
    zenith_calculator \
    zenith-calculator \
    os.zenith.Calculator.desktop \
    "ZenithOS calculator" \
    "python3, python3-gi, gir1.2-gtk-4.0, gir1.2-adw-1"

install_app_package \
    zenith-clock \
    zenith_clock \
    zenith-clock \
    os.zenith.Clock.desktop \
    "ZenithOS clock and timezone viewer" \
    "python3, python3-gi, gir1.2-gtk-4.0, gir1.2-adw-1"

install_app_package \
    zenith-text-editor \
    zenith_text_editor \
    zenith-text-editor \
    os.zenith.TextEditor.desktop \
    "ZenithOS text editor" \
    "python3, python3-gi, gir1.2-gtk-4.0, gir1.2-adw-1"

shell_root="$(pkg_root zenith-plasma-config)"
write_control \
    zenith-plasma-config \
    "ZenithShell KDE Plasma integration" \
    "plasma-desktop, plasma-workspace, kwin-wayland, systemsettings, plasma-nm, plasma-pa"
mkdir -p "$shell_root/etc/xdg"
mkdir -p "$shell_root/etc/skel/.config"
mkdir -p "$shell_root/usr/share/zenith"
cp "$repo_root/workstation/shell/plasma-config/kdeglobals" "$shell_root/etc/xdg/kdeglobals"
cp "$repo_root/workstation/shell/plasma-config/kwinrc" "$shell_root/etc/xdg/kwinrc"
cp "$repo_root/workstation/shell/plasma-config/kcminputrc" "$shell_root/etc/xdg/kcminputrc"
cp "$repo_root/workstation/shell/plasma-config/plasma-org.kde.plasma.desktop-appletsrc" \
    "$shell_root/etc/skel/.config/plasma-org.kde.plasma.desktop-appletsrc"
install -D -m 0644 "$repo_root/workstation/shell/plasma-config/zenith-panel-setup.js" \
    "$shell_root/usr/share/zenith/plasma/zenith-panel-setup.js"
cp "$repo_root/workstation/shell/plasma-config/metadata.desktop" "$shell_root/usr/share/zenith/metadata.desktop"

defaults_root="$(pkg_root zenith-workstation-defaults)"
write_control \
    zenith-workstation-defaults \
    "ZenithOS workstation defaults" \
    "zenith-plasma-config, zenith-welcome, zenith-settings, zenith-terminal, zenith-packages, zenith-files, zenith-installer, zenith-calculator, zenith-clock, zenith-text-editor, sddm, dconf-cli, systemd, sudo, plymouth, power-profiles-daemon, flatpak"
install -D -m 0644 "$repo_root/workstation/profiles/dev-vm.profile" "$defaults_root/usr/share/zenith/hardware-profiles/dev-vm.profile"
install -D -m 0644 "$repo_root/workstation/profiles/generic-workstation.profile" "$defaults_root/usr/share/zenith/hardware-profiles/generic-workstation.profile"
install -D -m 0644 "$repo_root/workstation/profiles/lenovo-v14-ada.profile" "$defaults_root/usr/share/zenith/hardware-profiles/lenovo-v14-ada.profile"
install -D -m 0644 "$repo_root/workstation/config/os-release" "$defaults_root/usr/share/zenith/defaults/os-release"
install -D -m 0644 "$repo_root/workstation/config/issue" "$defaults_root/usr/share/zenith/defaults/issue"
install -D -m 0644 "$repo_root/workstation/config/motd" "$defaults_root/usr/share/zenith/defaults/motd"
install -D -m 0644 "$repo_root/workstation/config/hostname" "$defaults_root/usr/share/zenith/defaults/hostname"
install -D -m 0644 "$repo_root/workstation/config/hosts" "$defaults_root/usr/share/zenith/defaults/hosts"
install -D -m 0644 "$repo_root/workstation/assets/backgrounds/zenith-default.svg" "$defaults_root/usr/share/backgrounds/zenith/zenith-default.svg"
install -D -m 0644 "$repo_root/workstation/assets/backgrounds/zenith-login-wallpaper.png" "$defaults_root/usr/share/backgrounds/zenith/zenith-login-wallpaper.png"
install -D -m 0644 "$repo_root/workstation/config/plymouth/zenith/zenith.plymouth" "$defaults_root/usr/share/plymouth/themes/zenith/zenith.plymouth"
install -D -m 0644 "$repo_root/workstation/config/plymouth/zenith/zenith.script" "$defaults_root/usr/share/plymouth/themes/zenith/zenith.script"

install -D -m 0644 "$repo_root/workstation/config/network/interfaces" "$defaults_root/etc/network/interfaces"
install -D -m 0644 "$repo_root/workstation/config/NetworkManager/conf.d/zenith-fast-boot.conf" "$defaults_root/etc/NetworkManager/conf.d/zenith-fast-boot.conf"
install -D -m 0644 "$repo_root/workstation/config/dconf/db/local.d/00-zenith" "$defaults_root/etc/dconf/db/local.d/00-zenith"
install -D -m 0644 "$repo_root/workstation/config/systemd/system.conf.d/zenith-boot.conf" "$defaults_root/etc/systemd/system.conf.d/zenith-boot.conf"
install -D -m 0644 "$repo_root/workstation/config/systemd/journald.conf.d/zenith-limits.conf" "$defaults_root/etc/systemd/journald.conf.d/zenith-limits.conf"
install -D -m 0440 "$repo_root/workstation/config/sudoers.d/90-zenith-live" "$defaults_root/etc/sudoers.d/90-zenith-live"
install -D -m 0644 "$repo_root/workstation/config/systemd/system/zenith-firstboot.service" "$defaults_root/etc/systemd/system/zenith-firstboot.service"
install -D -m 0644 "$repo_root/workstation/config/systemd/system/zenith-hardware-detect.service" "$defaults_root/etc/systemd/system/zenith-hardware-detect.service"
install -D -m 0644 "$repo_root/workstation/config/systemd/system/zenith-boot-report.service" "$defaults_root/etc/systemd/system/zenith-boot-report.service"
install -D -m 0644 "$repo_root/workstation/config/systemd/system/zenith-performance-defaults.service" "$defaults_root/etc/systemd/system/zenith-performance-defaults.service"
install -D -m 0644 "$repo_root/workstation/config/systemd/system/zenith-plymouth-handoff.service" "$defaults_root/etc/systemd/system/zenith-plymouth-handoff.service"
install -D -m 0644 "$repo_root/workstation/config/systemd/system/zenith-first-boot-apps.service" "$defaults_root/etc/systemd/system/zenith-first-boot-apps.service"
install -D -m 0644 "$repo_root/workstation/config/systemd/system/zenith-autoinstall.service" "$defaults_root/etc/systemd/system/zenith-autoinstall.service"
install -D -m 0644 "$repo_root/workstation/config/systemd/system/zenith-desktop-smoke-login.service" "$defaults_root/etc/systemd/system/zenith-desktop-smoke-login.service"
install -D -m 0644 "$repo_root/workstation/config/systemd/system/zenith-session-report-watch.service" "$defaults_root/etc/systemd/system/zenith-session-report-watch.service"
install -D -m 0755 "$repo_root/workstation/scripts/first-boot-apps.sh" "$defaults_root/usr/lib/zenith/first-boot-apps.sh"
install -D -m 0644 "$repo_root/workstation/config/sddm/sddm.conf" "$defaults_root/etc/sddm/sddm.conf"
mkdir -p "$defaults_root/usr/share/sddm/themes"
cp -R "$repo_root/workstation/themes/sddm/pixel-skyscrapers" "$defaults_root/usr/share/sddm/themes/pixel-skyscrapers"
install -D -m 0755 "$repo_root/workstation/config/usr/lib/zenith/zenith-boot-report" "$defaults_root/usr/lib/zenith/zenith-boot-report"
install -D -m 0755 "$repo_root/workstation/config/usr/lib/zenith/zenith-hardware-detect" "$defaults_root/usr/lib/zenith/zenith-hardware-detect"
install -D -m 0755 "$repo_root/workstation/config/usr/lib/zenith/zenith-performance-defaults" "$defaults_root/usr/lib/zenith/zenith-performance-defaults"
install -D -m 0755 "$repo_root/workstation/config/usr/lib/zenith/zenith-greeter" "$defaults_root/usr/lib/zenith/zenith-greeter"
install -D -m 0755 "$repo_root/workstation/config/usr/lib/zenith/zenith-session-setup" "$defaults_root/usr/lib/zenith/zenith-session-setup"
install -D -m 0755 "$repo_root/workstation/config/usr/lib/zenith/zenith-autoinstall" "$defaults_root/usr/lib/zenith/zenith-autoinstall"
install -D -m 0755 "$repo_root/workstation/config/usr/lib/zenith/zenith-desktop-smoke-login" "$defaults_root/usr/lib/zenith/zenith-desktop-smoke-login"
install -D -m 0755 "$repo_root/workstation/config/usr/lib/zenith/zenith-session-report" "$defaults_root/usr/lib/zenith/zenith-session-report"
install -D -m 0755 "$repo_root/workstation/config/usr/lib/zenith/zenith-session-report-watch" "$defaults_root/usr/lib/zenith/zenith-session-report-watch"
install -D -m 0755 "$repo_root/workstation/config/usr/lib/zenith/zenith-welcome-once" "$defaults_root/usr/lib/zenith/zenith-welcome-once"
install -D -m 0644 "$repo_root/workstation/config/xdg/autostart/zenith-greeter.desktop" "$defaults_root/etc/xdg/autostart/zenith-greeter.desktop"
install -D -m 0644 "$repo_root/workstation/config/xdg/autostart/zenith-session-setup.desktop" "$defaults_root/etc/xdg/autostart/zenith-session-setup.desktop"
install -D -m 0644 "$repo_root/workstation/config/xdg/autostart/zenith-session-report.desktop" "$defaults_root/etc/xdg/autostart/zenith-session-report.desktop"
install -D -m 0644 "$repo_root/workstation/config/xdg/autostart/zenith-welcome.desktop" "$defaults_root/etc/xdg/autostart/zenith-welcome.desktop"
install -D -m 0644 "$repo_root/workstation/config/sddm/sddm.conf" "$defaults_root/usr/share/zenith/defaults/sddm.conf"
install -D -m 0644 "$repo_root/workstation/config/sddm/sddm.conf" "$defaults_root/etc/sddm.conf"
cat > "$defaults_root/DEBIAN/postinst" <<'EOF'
#!/bin/sh
set -e

if [ -f /usr/share/zenith/defaults/os-release ]; then
    rm -f /etc/os-release
    install -m 0644 /usr/share/zenith/defaults/os-release /etc/os-release
fi
if [ -f /usr/share/zenith/defaults/issue ]; then
    install -m 0644 /usr/share/zenith/defaults/issue /etc/issue
fi
if [ -f /usr/share/zenith/defaults/motd ]; then
    install -m 0644 /usr/share/zenith/defaults/motd /etc/motd
fi
if [ -f /usr/share/zenith/defaults/hostname ]; then
    install -m 0644 /usr/share/zenith/defaults/hostname /etc/hostname
fi
if [ -f /usr/share/zenith/defaults/hosts ]; then
    install -m 0644 /usr/share/zenith/defaults/hosts /etc/hosts
fi
if [ -f /usr/share/zenith/defaults/sddm.conf ]; then
    install -m 0644 /usr/share/zenith/defaults/sddm.conf /etc/sddm.conf
    mkdir -p /etc/sddm
    install -m 0644 /usr/share/zenith/defaults/sddm.conf /etc/sddm/sddm.conf
fi
mkdir -p /etc/plymouth
cat > /etc/plymouth/plymouthd.conf <<PLYMOUTH
[Daemon]
Theme=zenith
ShowDelay=0
PLYMOUTH

if command -v systemctl >/dev/null 2>&1; then
    systemctl mask live-config.service >/dev/null 2>&1 || true
    systemctl mask NetworkManager-wait-online.service systemd-networkd-wait-online.service >/dev/null 2>&1 || true
    systemctl disable NetworkManager-wait-online.service systemd-networkd-wait-online.service >/dev/null 2>&1 || true
    systemctl enable zenith-hardware-detect.service >/dev/null 2>&1 || true
    systemctl enable zenith-performance-defaults.service >/dev/null 2>&1 || true
fi
if command -v dconf >/dev/null 2>&1; then
    dconf update >/dev/null 2>&1 || true
fi
rm -f \
    /etc/systemd/system/network-online.target.wants/NetworkManager-wait-online.service \
    /etc/systemd/system/network-online.target.wants/systemd-networkd-wait-online.service
touch /etc/.updated /var/.updated

exit 0
EOF
chmod 0755 "$defaults_root/DEBIAN/postinst"
mkdir -p "$defaults_root/usr/share/zenith/themes"
cp -R "$repo_root/workstation/themes/." "$defaults_root/usr/share/zenith/themes/"

builder_root="$(pkg_root zenith-builder)"
write_control \
    zenith-builder \
    "ZenithOS self-hosted image builder" \
    "bash, python3, make, git, rsync, mmdebstrap, debootstrap, squashfs-tools, xorriso, grub-common, grub-pc-bin, grub-efi-amd64-bin, qemu-system-x86"
install -D -m 0755 "$repo_root/workstation/selfhost/zenith-build.sh" "$builder_root/usr/bin/zenith-build"
install -D -m 0644 "$repo_root/workstation/selfhost/zenith-build.desktop" "$builder_root/usr/share/applications/zenith-build.desktop"

for package in \
    zenith-settings \
    zenith-terminal \
    zenith-packages \
    zenith-files \
    zenith-welcome \
    zenith-installer \
    zenith-calculator \
    zenith-clock \
    zenith-text-editor \
    zenith-plasma-config \
    zenith-workstation-defaults \
    zenith-builder; do
    build_package "$package"
done
