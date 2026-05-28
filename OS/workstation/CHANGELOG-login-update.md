# ZenithOS Login Configuration Update

## Summary
**Date:** 2026-05-26  
**Change:** Disabled autologin and configured secure manual login credentials

## Changes Made

### 1. GDM Configuration (`workstation/config/gdm/daemon.conf`)
- **Before:** `AutomaticLoginEnable=true` with user `zenith`
- **After:** `AutomaticLoginEnable=false` (manual login required)
- **Security:** Autologin disabled for production security

### 2. User Account Setup (`workstation/iso/build-rootfs.sh`)
- **Username:** `hailtheking`
- **Password:** `2976880801`
- **Groups:** sudo, adm, audio, video, plugdev, netdev
- **Shell:** /bin/bash
- **Home:** /home/hailtheking

### 3. Login Screen Configuration (`workstation/config/dconf/db/gdm.d/00-zenith-login`)
- Banner message: **Disabled** (was showing "ZenithOS Workstation")
- User list: **Visible** (disable-user-list=false)
- Clean login interface for manual credential entry

### 4. Documentation Updates (`workstation/README.md`)
- Updated login instructions with new credentials
- Changed from autologin description to manual login steps
- Added password change reminder: `passwd`

### 5. Runtime Validation (`workstation/qa/runtime-check.sh`)
- Updated checks to verify `AutomaticLoginEnable=false`
- Updated checks to verify `banner-message-enable=false`
- Ensures configuration is correctly applied in builds

## Files Modified

1. `/mnt/c/Users/krish/ZenithOS/OS/workstation/config/gdm/daemon.conf`
2. `/mnt/c/Users/krish/ZenithOS/OS/workstation/iso/build-rootfs.sh`
3. `/mnt/c/Users/krish/ZenithOS/OS/workstation/config/dconf/db/gdm.d/00-zenith-login`
4. `/mnt/c/Users/krish/ZenithOS/OS/workstation/README.md`
5. `/mnt/c/Users/krish/ZenithOS/OS/workstation/qa/runtime-check.sh`

## Login Instructions

### At the GDM Login Screen:
1. Select **hailtheking** from the user list
2. Enter password: **2976880801**
3. Press Enter or click "Log In"

### After First Login:
```bash
# Change the default password (recommended)
passwd

# Verify group memberships
groups
# Should show: hailtheking sudo adm audio video plugdev netdev
```

## Build & Test

### Rebuild the rootfs with new credentials:
```bash
cd /mnt/c/Users/krish/ZenithOS/OS
make workstation-rootfs
```

### Validate configuration:
```bash
make workstation-check
make workstation-verify-repo
```

### Test in QEMU:
```bash
# Direct kernel boot (fast)
make workstation-run-seed

# Full ISO boot
make workstation-run-seed-iso
```

## Security Notes

- ✅ Autologin disabled - manual authentication required
- ✅ User has sudo privileges for administration
- ✅ Password should be changed after first login
- ✅ GDM banner removed - no system information displayed
- ✅ User list visible for easier login (single-user system)

## Rollback (if needed)

To restore autologin with zenith user:
```bash
# In workstation/config/gdm/daemon.conf:
AutomaticLoginEnable=true
AutomaticLogin=zenith
```

## Next Steps

1. **Rebuild rootfs:** `make workstation-rootfs`
2. **Rebuild ISO:** `make workstation-seed-iso`
3. **Test login:** Boot in QEMU and verify credentials work
4. **Update documentation:** Any other references to zenith user

## Contact

For questions about this change, refer to the ZenithOS project documentation or contact the development team.
