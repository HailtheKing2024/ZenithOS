# ZenithOS OS Building Guide

**Comprehensive reference for building custom Linux distributions**

---

## 📚 Research Summary: OS Building Best Practices

### Key Resources Studied:
1. **Linux From Scratch (LFS)** - Complete guide to building Linux from source
2. **Debian Live Build** - Tool for creating custom Debian-based live systems
3. **Archiso** - Arch Linux ISO creation tool
4. **Yocto Project** - Industrial-grade embedded Linux build system
5. **Buildroot** - Simple embedded Linux build system

### Core Principles Learned:

#### 1. **Package Management Strategy**
- ✅ Use existing package managers (apt, dnf, pacman) for base system
- ✅ Create custom packages for distribution-specific features
- ✅ Support multiple sources: official repos + Flatpak/Snap for apps
- ✅ Keep base image small, install apps post-boot or on-demand

#### 2. **Build System Architecture**
```
┌─────────────────────────────────────────┐
│  Source Code & Configuration            │
│  - Kernel source                        │
│  - Package manifests                    │
│  - Config files                         │
│  - Custom scripts                       │
└─────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────┐
│  Bootstrap (mmdebstrap/debootstrap)     │
│  - Minimal base system                  │
│  - Package installation                 │
│  - Repository configuration             │
└─────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────┐
│  Customization (chroot)                 │
│  - User accounts                        │
│  - System configuration                 │
│  - Service enablement                   │
│  - Theme installation                   │
└─────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────┐
│  Image Creation                         │
│  - SquashFS for rootfs                  │
│  - ISO/IMG generation                   │
│  - Bootloader installation              │
│  - Metadata embedding                   │
└─────────────────────────────────────────┘
```

#### 3. **Application Distribution Strategies**

**Option A: Pre-installed in ISO**
- ✅ Pros: Works offline immediately, consistent experience
- ❌ Cons: Larger ISO, apps may be outdated by install time

**Option B: Post-install via Package Manager**
- ✅ Pros: Smaller ISO, latest versions, user choice
- ❌ Cons: Requires internet, extra step for user

**Option C: First-boot Installation (ZenithOS Choice)**
- ✅ Pros: Best of both - small ISO + auto-install common apps
- ✅ Apps are latest version from Flathub
- ✅ User can skip if offline
- ❌ Cons: First boot takes longer

#### 4. **Common Pitfalls Avoided**
- ❌ Don't hardcode proprietary software (use Flatpak)
- ❌ Don't skip license verification
- ❌ Don't ignore secure boot (but can defer for dev)
- ❌ Don't build everything from scratch (use base distro)
- ✅ Do validate checksums
- ✅ Do test on real hardware early
- ✅ Do document build requirements

---

## 🛠️ ZenithOS Build System

### Current Architecture:
```
ZenithOS/
├── workstation/
│   ├── apps/              # First-party applications
│   ├── config/            # System configuration files
│   ├── iso/               # ISO build scripts
│   ├── packages/          # Package manifests and DEB builders
│   ├── themes/            # SDDM/GNOME themes
│   └── scripts/           # Utility scripts
├── kernel-lab/            # From-scratch kernel
└── tools/                 # Build and testing tools
```

### Build Commands:
```bash
# Complete rebuild
make workstation-clean
make workstation-rootfs
make workstation-seed-iso

# Individual components
make workstation-repo      # Build packages + apt repo
make workstation-rootfs    # Create rootfs with mmdebstrap
make workstation-seed-iso  # Generate bootable ISO
```

### Base System:
- **Base:** Debian Trixie (testing)
- **Package Manager:** apt + Flatpak
- **Display Manager:** SDDM
- **Desktop:** GNOME 48 + Zenith extensions
- **Init:** systemd
- **Kernel:** Linux AMD64 (from Debian)

---

## 📦 Adding Applications to ZenithOS

### Method 1: Flatpak (Recommended for Proprietary Apps)

**Why Flatpak?**
- ✅ Auto-updating
- ✅ Sandboxed/security
- ✅ Works across Linux distributions
- ✅ No license redistribution issues
- ✅ Smaller base ISO

**Apps Added via Flatpak (first-boot script):**
```bash
# Code Editors
flatpak install flathub com.visualstudio.code
flatpak install flathub org.jetbrains.IntelliJ-IDEA-Community

# Browsers
flatpak install flathub com.google.Chrome
flatpak install flathub org.mozilla.firefox
flatpak install flathub com.brave.Browser

# Communication
flatpak install flathub com.discordapp.Discord
flatpak install flathub org.telegram.desktop

# Media
flatpak install flathub com.spotify.Client
flatpak install flathub org.videolan.VLC

# Development
flatpak install flathub org.gnome.Builder
flatpak install flathub io.dbeaver.DBeaverCommunity
```

**Location:** `/mnt/c/Users/krish/ZenithOS/OS/workstation/scripts/first-boot-apps.sh`

**Service:** `zenith-first-boot-apps.service` (runs once on first boot)

### Method 2: APT Packages (For System Software)

**Edit:** `workstation/packages/rootfs.apt.manifest`

```bash
# Example additions:
vim
git
htop
tmux
build-essential
```

**Note:** Only add FOSS software that can be freely redistributed.

### Method 3: Custom DEB Packages (For ZenithOS Features)

**Location:** `workstation/apps/`

**Structure:**
```
zenith-appname/
├── src/zenith_appname/   # Python application code
├── bin/zenith-appname    # Launcher script
└── desktop/os.zenith.AppName.desktop
```

**Build:** Automatically built by `build-zenith-packages.sh`

---

## 🎨 Theme Integration

### SDDM Themes:
**Location:** `/usr/share/sddm/themes/<theme-name>/`

**Required Files:**
- `metadata.desktop` - Theme info
- `Main.qml` - Main UI
- `theme.conf` - Configuration
- Assets (fonts, images, videos)

**Current Theme:** pixel-skyscrapers from Darkkal44/qylock

**Integration Steps:**
1. Download theme to `workstation/themes/sddm/`
2. Add all assets (including fonts, videos)
3. Configure in `workstation/config/sddm/sddm.conf`
4. Rebuild ISO so assets are in squashfs

### GNOME Themes:
**Location:** `/usr/share/themes/` or `~/.themes/`

**Tools:** Gnome Tweaks, Extension Manager

---

## 🔧 Advanced Customization

### Kernel Customization:
1. Download kernel source from kernel.org
2. Configure with `make menuconfig`
3. Build with `make -j$(nproc)`
4. Create custom DEB package
5. Install in rootfs

### Custom Init Scripts:
**Location:** `workstation/config/systemd/system/`

**Enable in build:** Add to `build-rootfs.sh`:
```bash
sudo chroot "$output" systemctl enable my-custom.service
```

### Hardware Profiles:
**Location:** `workstation/config/usr/lib/zenith/`

**Examples:**
- `zenith-hardware-detect` - Detects hardware on first boot
- `zenith-performance-defaults` - Sets CPU governor, swappiness
- `zenith-greeter` - Custom login greetings

---

## 📊 Build Optimization Tips

### Reduce ISO Size:
1. Use Flatpak for large apps (Chrome = 200MB as Flatpak vs bloating ISO)
2. Remove unnecessary firmware
3. Compress with zstd instead of gzip
4. Strip debug symbols from non-dev packages

### Improve Build Speed:
1. Use apt-cacher-ng for package caching
2. Keep package-work directory between builds
3. Use ccache for kernel/package compilation
4. Build in RAM if you have enough memory

### Quality Assurance:
1. Automated testing with QEMU
2. Check ISO checksums
3. Test on real hardware early
4. Validate all custom packages with lintian

---

## 🚀 Next Steps for ZenithOS

### Immediate:
1. ✅ SDDM theme integration (in progress)
2. ✅ First-boot app installer (completed)
3. ⏳ Hardware-specific optimizations for Lenovo V14-ADA
4. ⏳ Performance tuning scripts

### Short-term:
1. Custom ZenithOS wallpapers and branding
2. Improved installer with partitioning wizard
3. Snapshot/rollback with snapper
4. Automated testing pipeline

### Long-term:
1. Hybrid kernel development (Kernel Lab track)
2. Custom compositor (Wayland-based)
3. Zenith package repository expansion
4. ARM port for Raspberry Pi / Apple Silicon

---

## 📖 Additional Resources

### Documentation:
- Debian Live Manual: https://debian-live.alioth.debian.org/live-manual/
- Arch Wiki: https://wiki.archlinux.org/
- OSDev Wiki: https://wiki.osdev.org/
- Linux From Scratch: http://www.linuxfromscratch.org/

### Tools:
- mkosi: https://github.com/systemd/mkosi (modern image builder)
- debootstrap/mmdebstrap: Debian rootfs creation
- SquashFS Tools: Compressed filesystem creation
- xorriso: ISO creation

### Communities:
- r/linuxfromscratch (Reddit)
- OSDev Forums
- Debian Developers mailing list
- Arch Linux Forums

---

**Last Updated:** 2026-05-27  
**Author:** ZenithOS Build Team  
**License:** MIT (documentation), various (code/package licenses)