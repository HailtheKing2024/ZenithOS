# ZenithOS 30GB Storage Enforcement

Owner: `@zenith-pkg`

The workstation image must remain usable on a 30GB target disk. Package tools should treat the storage budget as a product contract, not as a reporting-only target.

## Budget Summary

- Total target: 30GB.
- User data reserve: 8.75GB.
- System root: 10.5GB.
- Variable package, service state, and self-host build metadata: 4.75GB.
- System Flatpak runtimes and apps: 4GB.
- Rollback snapshot reserve: 1GB.
- EFI and recovery slack: 1GB total.

## Enforcement Points

1. Pre-transaction check
   - `apt`, graphical updates, and Flatpak system installs must estimate installed size, download cache growth, and post-install trigger growth.
   - Transactions that would consume the 8.75GB user data reserve are denied by default.

2. APT cache control
   - `/var/cache/apt/archives` is capped at 1536MB.
   - Successful graphical updates should run the equivalent of `apt clean` for superseded package archives.
   - `apt autoremove` is exposed in the graphical updater and recommended when orphaned packages exceed 512MB.

3. Flatpak runtime control
   - System Flatpak storage is capped at 4GB.
   - Only one GNOME runtime branch is retained by default.
   - Unused runtimes are pruned after successful updates.
   - User-installed Flatpak apps count against user data, not the base OS budget.

4. Logs and crash dumps
   - Structured logs are capped at 512MB total.
   - Crash dumps are capped at 256MB total and retained newest-first.
   - Diagnostic bundles must be exportable before deletion, but export copies count against user data.

5. Rollback
   - Keep one known-good system rollback snapshot inside the 30GB budget.
   - Additional snapshots require an administrator override and clear UI disclosure of consumed space.

6. Self-hosted builds
   - Build outputs live under `/var/lib/zenith-build` by default.
   - Build outputs are capped at 6GB unless an administrator explicitly overrides the limit.
   - Zenith Builder must show cleanup actions for old rootfs and ISO artifacts.

## Failure Behavior

- Package managers must fail closed when budget data is unavailable.
- Overrides must be explicit, logged, and scoped to one transaction.
- The shell updater must show which budget was exceeded: host packages, APT cache, Flatpak system storage, logs, crash dumps, rollback, self-hosted build outputs, or user data reserve.
