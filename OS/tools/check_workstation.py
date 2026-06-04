#!/usr/bin/env python3
import json
import pathlib
import sys


ROOT = pathlib.Path(__file__).resolve().parents[1]


REQUIRED_FILES = [
    "README.md",
    "plan.json",
    "storage.json",
    "threat_model.json",
    "boot_sequence.md",
    "hardware_matrix.md",
    "workstation/README.md",
    "workstation/architecture.md",
    "workstation/roadmap.md",
    "workstation/release/CHANGELOG.md",
    "workstation/release/KNOWN_ISSUES.md",
    "workstation/release/RELEASE_NOTES.md",
    "workstation/distro/README.md",
    "workstation/distro/pipeline.md",
    "workstation/distro/gnome-source-plan.md",
    "workstation/distro/package-set.manifest",
    "workstation/distro/build-zenith-packages.sh",
    "workstation/distro/publish-apt-repo.sh",
    "workstation/distro/verify-apt-repo.sh",
    "workstation/packages/strategy.md",
    "workstation/packages/base.include.manifest",
    "workstation/packages/rootfs.apt.manifest",
    "workstation/packages/exclude.manifest",
    "workstation/packages/flatpak.include.manifest",
    "workstation/storage/budget.json",
    "workstation/storage/enforcement.md",
    "workstation/storage/enforcement-policy.manifest",
    "workstation/profiles/README.md",
    "workstation/profiles/dev-vm.profile",
    "workstation/profiles/generic-workstation.profile",
    "workstation/profiles/lenovo-v14-ada.profile",
    "workstation/shell/shell-spec.md",
    "workstation/shell/plasma-config/metadata.desktop",
    "workstation/shell/plasma-config/plasma-org.kde.plasma.desktop-appletsrc",
    "workstation/shell/plasma-config/kdeglobals",
    "workstation/shell/plasma-config/kwinrc",
    "workstation/shell/plasma-config/kcminputrc",
    "workstation/apps/zenith-settings/pyproject.toml",
    "workstation/apps/zenith-settings/src/zenith_settings/main.py",
    "workstation/apps/zenith-terminal/pyproject.toml",
    "workstation/apps/zenith-terminal/src/zenith_terminal/main.py",
    "workstation/apps/zenith-packages/pyproject.toml",
    "workstation/apps/zenith-packages/src/zenith_packages/main.py",
    "workstation/apps/zenith-files/pyproject.toml",
    "workstation/apps/zenith-files/src/zenith_files/main.py",
    "workstation/apps/zenith-welcome/pyproject.toml",
    "workstation/apps/zenith-welcome/src/zenith_welcome/main.py",
    "workstation/apps/zenith-installer/pyproject.toml",
    "workstation/apps/zenith-installer/src/zenith_installer/main.py",
    "workstation/apps/bin/zenith-settings",
    "workstation/apps/bin/zenith-terminal",
    "workstation/apps/bin/zenith-packages",
    "workstation/apps/bin/zenith-files",
    "workstation/apps/bin/zenith-welcome",
    "workstation/apps/bin/zenith-installer",
    "workstation/apps/desktop/os.zenith.Settings.desktop",
    "workstation/apps/desktop/os.zenith.Terminal.desktop",
    "workstation/apps/desktop/os.zenith.Packages.desktop",
    "workstation/apps/desktop/os.zenith.Files.desktop",
    "workstation/apps/desktop/os.zenith.Welcome.desktop",
    "workstation/apps/desktop/os.zenith.Installer.desktop",
    "workstation/assets/backgrounds/zenith-default.svg",
    "workstation/config/os-release",
    "workstation/config/issue",
    "workstation/config/motd",
    "workstation/config/sddm/sddm.conf",
    "workstation/config/network/interfaces",
    "workstation/config/plymouth/zenith/zenith.plymouth",
    "workstation/config/plymouth/zenith/zenith.script",
    "workstation/config/dconf/db/local.d/00-zenith",
    "workstation/config/sudoers.d/90-zenith-live",
    "workstation/config/systemd/system/zenith-firstboot.service",
    "workstation/config/systemd/system/zenith-hardware-detect.service",
    "workstation/config/systemd/system/zenith-plymouth-handoff.service",
    "workstation/config/usr/lib/zenith/zenith-hardware-detect",
    "workstation/config/usr/lib/zenith/zenith-session-setup",
    "workstation/config/usr/lib/zenith/zenith-welcome-once",
    "workstation/config/xdg/autostart/zenith-session-setup.desktop",
    "workstation/config/xdg/autostart/zenith-welcome.desktop",
    "workstation/themes/zenith.css",
    "workstation/iso/build-rootfs.sh",
    "workstation/iso/build-live-iso.sh",
    "workstation/iso/build-seed-iso.sh",
    "workstation/iso/create-persistence-disk.sh",
    "workstation/iso/check-apt-packages.sh",
    "workstation/qa/runtime-check.sh",
    "workstation/selfhost/README.md",
    "workstation/selfhost/sources.exclude",
    "workstation/selfhost/zenith-build.sh",
    "workstation/selfhost/zenith-build.desktop",
    "tools/run_workstation_seed.ps1",
    "tools/smoke_workstation_seed.ps1",
    "tools/qemu_diagnostic_matrix.ps1",
    "docs/kernel-lab.md",
]


def fail(message):
    print(f"workstation-check: {message}", file=sys.stderr)
    raise SystemExit(1)


def load_json(path):
    try:
        return json.loads((ROOT / path).read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        fail(f"{path} is invalid JSON: {exc}")


def uncommented_lines(path):
    lines = []
    for raw in (ROOT / path).read_text(encoding="utf-8").splitlines():
        line = raw.split("#", 1)[0].strip()
        if line:
            lines.append(line)
    return lines


def check_required_files():
    missing = [path for path in REQUIRED_FILES if not (ROOT / path).exists()]
    if missing:
        fail("missing required files: " + ", ".join(missing))


def check_root_budget():
    data = load_json("storage.json")
    target = float(data["target_gb"])
    total = sum(float(value) for value in data["budget"].values())
    if total > target + 0.001:
        fail(f"storage.json budget is {total:.2f}GB, target is {target:.2f}GB")


def check_workstation_budget():
    data = load_json("workstation/storage/budget.json")
    target = float(data["target_gb"])
    partitions = data["partitions"]
    total = float(data["reserved_user_data_gb"])
    total += float(partitions["efi_system_mb"]) / 1024.0
    total += sum(float(value) for key, value in partitions.items() if key != "efi_system_mb")
    if total > target + 0.001:
        fail(f"workstation storage budget is {total:.2f}GB, target is {target:.2f}GB")


def check_plan_graph():
    plan = load_json("plan.json")
    nodes = {item["id"]: set(item.get("depends_on", [])) for item in plan["subsystems"]}
    for node, deps in nodes.items():
        missing = deps - nodes.keys()
        if missing:
            fail(f"plan node {node} depends on missing nodes: {sorted(missing)}")

    visiting = set()
    visited = set()

    def visit(node):
        if node in visiting:
            fail(f"dependency cycle includes {node}")
        if node in visited:
            return
        visiting.add(node)
        for dep in nodes[node]:
            visit(dep)
        visiting.remove(node)
        visited.add(node)

    for node in nodes:
        visit(node)


def check_package_policy():
    rootfs = set(uncommented_lines("workstation/packages/rootfs.apt.manifest"))
    required = {
        "apt",
        "dpkg",
        "systemd",
        "zstd",
        "polkitd",
        "pkexec",
        "plasma-desktop",
        "kwin-wayland",
        "flatpak",
        "pipewire",
        "live-boot",
        "plymouth",
        "plymouth-themes",
        "gir1.2-vte-3.91",
        "make",
        "git",
        "rsync",
        "dpkg-dev",
        "fakeroot",
        "mmdebstrap",
        "squashfs-tools",
        "xorriso",
        "grub-efi-amd64-bin",
        "grub-common",
    }
    missing = sorted(required - rootfs)
    if missing:
        fail("rootfs apt manifest missing required packages: " + ", ".join(missing))

    forbidden = {"dnf", "rpm", "pacman", "snapd"}
    present = sorted(forbidden & rootfs)
    if present:
        fail("rootfs apt manifest includes forbidden package managers: " + ", ".join(present))

    excludes = "\n".join(uncommented_lines("workstation/packages/exclude.manifest"))
    if "exclude:full-plasma-desktop" in excludes or "exclude:full-kwin-wayland" in excludes:
        fail("exclude manifest blocks KDE Plasma internals needed by the chosen architecture")

    zenith_packages = set(uncommented_lines("workstation/distro/package-set.manifest"))
    expected = {
        "zenith-workstation-defaults",
        "zenith-plasma-config",
        "zenith-settings",
        "zenith-terminal",
        "zenith-packages",
        "zenith-files",
        "zenith-welcome",
        "zenith-installer",
        "zenith-builder",
    }
    missing_zenith = sorted(expected - zenith_packages)
    if missing_zenith:
        fail("Zenith package set missing: " + ", ".join(missing_zenith))


def main():
    check_required_files()
    check_root_budget()
    check_workstation_budget()
    check_plan_graph()
    check_package_policy()
    print("workstation-check: ok")


if __name__ == "__main__":
    main()
