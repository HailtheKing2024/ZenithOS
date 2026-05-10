# ZenithOS Distribution Pipeline

ZenithOS follows the same release shape as large Linux distributions:

```text
source tree
-> package build
-> apt repository
-> image compose
-> QA boot test
-> release
```

Ubuntu uses Launchpad builders for package builds, Debian uses buildd/sbuild,
and Fedora uses Koji/Mock plus Pungi for compose. ZenithOS uses the Debian
package format because the OS package manager is apt/dpkg.

## Current Stage

The first stage packages Zenith-owned components:

- Zenith apps
- ZenithShell extension
- Zenith defaults
- Zenith Builder

The next stage is source-building GNOME platform packages into Zenith-owned
`.deb` packages instead of consuming GNOME binaries directly from an upstream
distro repository.

## Directories

```text
distro/
  build-zenith-packages.sh   Build Zenith-owned .deb packages.
  publish-apt-repo.sh        Publish built packages into a local apt repo.
  package-set.manifest       Packages installed into the Workstation image.
  gnome-source-plan.md       Long-term source-built GNOME plan.
```
