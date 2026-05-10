# ZenithOS Workstation Package Strategy

Owner: `@zenith-pkg`

Scope: user-space package strategy only. Kernel, boot, scheduler, syscall, and existing userland source files are out of scope for this package layer.

## Goals

- Use `apt` and `dpkg` as the host package foundation because they provide mature dependency resolution, signed repository metadata, trigger support, maintainer scripts, and a large existing packaging ecosystem.
- Support Flatpak as the sandboxed desktop application runtime while avoiding duplicated runtimes in the default install.
- Track GNOME internals as architecture references for a Fedora-like workstation shell without copying GNOME source or relying on GNOME as the ZenithOS shell implementation.
- Keep a complete workstation install inside a 30GB disk target, including a reserve for user data.

## Package Layers

1. `zenith-base`
   - Bootstraps the package database, root filesystem layout, certificates, locale data, policy files, and package signing keys.
   - Owns `/etc/apt`, `/var/lib/dpkg`, `/var/lib/apt`, and the base repository trust anchors.

2. `zenith-posix-core`
   - Provides POSIX-compatible user-space tools, shell utilities, process tools, text tools, archive tools, compression tools, and basic networking CLI.
   - Depends only on stable C library, dynamic linker, filesystem layout, and service supervision ABI.

3. `zenith-desktop-core`
   - Provides the workstation session manager, settings service, notification service, portal broker, font stack, icon themes, MIME database, and desktop schemas.
   - Uses GNOME internals as interface references, especially D-Bus service boundaries, settings schemas, portals, MIME handling, and session lifecycle.

4. `zenith-flatpak-runtime`
   - Provides Flatpak, xdg-desktop-portal compatibility, OSTree storage policy, app permission mediation, and runtime pruning hooks.
   - Installed by default, but only the minimum runtime set is allowed in the base image.

5. `zenith-workstation-defaults`
   - Installs curated default applications and desktop integration packages.
   - Must not pull heavyweight duplicate stacks when a shared system component is already present.

## APT/DPKG Base

- ZenithOS repositories should be signed with release metadata verification enabled before any package payload is accepted.
- `dpkg` remains the ground-truth local package database for host packages.
- `apt` is the only dependency solver exposed to users and graphical update tools.
- Host packages must use deterministic maintainer scripts and avoid probing kernel internals directly. Hardware integration goes through stable ZenithOS service APIs.
- Package triggers are allowed for font caches, MIME caches, icon caches, desktop database updates, schema compilation, and initramfs-equivalent image refreshes.
- Package post-install actions must be idempotent and must not require network access.

## Flatpak Support

- Flatpak is supported for third-party GUI applications and apps with frequent release cadence.
- The default workstation image should install Flatpak tooling and portal services, not a large set of preinstalled Flatpak apps.
- Runtime duplication is controlled by a strict runtime allowlist and automatic pruning of unused runtimes.
- Flatpak app data counts against the user data reserve unless installed system-wide by an administrator.
- System portals must mediate filesystem access, camera, microphone, notifications, printing, screenshots, secrets, and open-file flows.

## GNOME Internals Mapping

ZenithOS should study GNOME boundaries and behavior, then implement original components:

- `gnome-shell` reference area: session shell composition, overview behavior, notifications, extensions policy.
- `mutter` reference area: compositor responsibilities, monitor configuration, input routing, frame scheduling.
- `gsettings` and `dconf` reference area: settings schema model and low-latency settings reads.
- `xdg-desktop-portal` reference area: sandbox permission flows and user-mediated resource grants.
- `accountsservice` reference area: user identity metadata exposed to login/session components.
- `NetworkManager` reference area: network state service shape, not a required copied implementation.
- `PackageKit` reference area: graphical update abstraction over `apt`.

This layer must preserve the zero-plagiarism rule: interface concepts and public protocol behavior are acceptable references; source code and private implementation details are not copied.

## Include/Exclude Policy

- Include manifests define what belongs in the default workstation install.
- Exclude manifests define package families that are incompatible with the product direction, duplicate base functionality, or exceed the 30GB budget.
- Every default package must fit one of these categories: base system, POSIX compatibility, desktop runtime, workstation UX, Flatpak support, diagnostics, accessibility, or hardware enablement.
- Packages outside those categories require explicit product justification and storage impact review.

## Storage Enforcement

- The whole workstation target is capped at 30GB.
- APT package archives are capped separately from installed package size.
- Logs, crash dumps, Flatpak runtimes, and rollback snapshots have explicit caps.
- Package installation tooling must fail closed when a requested transaction would violate reserved user-data space.
