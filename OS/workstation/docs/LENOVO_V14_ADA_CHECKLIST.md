# ZenithOS Lenovo V14-ADA Hardware Validation Checklist

Use this checklist when booting the ZenithOS seed ISO on the physical Lenovo V14-ADA.

## Preparation

1. Download the latest seed ISO from the build artifacts (or local build)
2. Flash to USB using Ventoy or `dd`
3. Boot the V14-ADA from USB (F12 for boot menu)
4. Login: `hailtheking` / `2976880801` (or any created user)

## Boot Tests

| # | Test | Expected | Pass/Fail |
|---|------|----------|-----------|
| 1 | ISO boots to SDDM (pixel-skyscrapers theme) | Animated background, custom font | |
| 2 | Login succeeds | KDE Plasma Wayland session starts | |
| 3 | Plymouth splash displays | Zenith logo animation | |
| 4 | Boot time < 60s to SDDM | Stopwatch from GRUB to SDDM | |
| 5 | UEFI secure boot | No shim/MOK enrollment errors | |

## Display

| # | Test | Expected | Pass/Fail |
|---|------|----------|-----------|
| 6 | Internal display at native res (1920x1080) | Sharp, no tearing | |
| 7 | Brightness keys work (Fn+F5/F6) | OSD shows brightness level | |
| 8 | External HDMI output | Second display detected | |
| 9 | Display color/quality | No washed out/flickering | |
| 10 | Night light (if enabled) | Warm tone at night | |

## Touchpad

| # | Test | Expected | Pass/Fail |
|---|------|----------|-----------|
| 11 | Basic pointer movement | Smooth, no jumps | |
| 12 | Two-finger scroll | Smooth in both directions | |
| 13 | Tap-to-click | Works (if enabled) | |
| 14 | Three-finger gestures | Switch desktops/applications | |
| 15 | Palm rejection while typing | No cursor drift | |

## Wi-Fi

| # | Test |Authenticator Expected | Pass/Fail |
|---|------|----------|-----------|
| 16 | Wi-Fi card detected (Realtek/Intel/AMD?) | `nmcli device` shows wlan0 | |
| 17 | Connect to WPA2 network | Stable, good speed | |
| 18 | Speed test (fast.com) | >50 Mbps typical | |
| 19 | Suspend/resume keeps connection | Reconnects automatically | |
| 20 | Wi-Fi dropdown in Zenith Settings | Shows networks, toggles on/off | |

## Audio

| # | Test | Expected | Pass/Fail |
|---|------|----------|-----------|
| 21 | Speakers produce sound | Test: `speaker-test -t wav` or media | |
| 22 | Headphone jack detection | Auto-switches output | |
| 23 | Microphone input | Records and plays back | |
| 24 | Bluetooth audio (if available) | Pairs, plays, no crackle | |
| 25 | Volume keys work (Fn+F2/F3) | OSD shows volume level | |

## Storage

| # | Test | Expected | Pass/Fail |
|---|------|----------|-----------|
| 26 | Internal SSD detected | `lsblk` shows NVMe/SATA | |
| 27 | USB flash drive mounts | Auto-mounts, read/write | |
| 28 | Installer detects target disk | Shows in Zenith Installer | |
| 29 | Install to Btrfs completes | EFI + @ + @home + @var + @snapshots | |
| 30 | Boot from installed disk | Reboots into installed ZenithOS | |

## Suspend/Resume / Power

| # | Test | Expected | Pass/Fail |
|---|------|----------|-----------|
| 31 | Lid close suspends | Power LED blinks, quiet | |
| 32 | Lid open resumes | Returns to lock screen | |
| 33 | Power button suspend | Same as lid close | |
| 34 | Battery status accurate | Matches hardware indicator | |
| 35 | Power profile switch (Zenith Settings) | Changes governor/behavior | |
| 36 | Thermal management | No >85°C under normal load | |

## Extras

| # | Test | Expected | Pass/Fail |
|---|------|----------|-----------|
| 37 | Bluetooth device pairs | Mouse/keyboard works | |
| 38 | Webcam (if present) | Cheese or browser sees it | |
| 39 | First-boot Flatpak apps install | VS Code, Chrome, etc. (needs network) | |
| 40 | Zenith session report | `cat ~/.cache/zenith-session-report.log` shows PASS | |

## Collect Probe Data

After testing, run on the V14-ADA:

```bash
sudo bash /usr/lib/zenith/zenith-hardware-probe > /tmp/zenith-hw-probe-$(date +%s).log 2>&1
cp /tmp/zenith-hw-probe-*.log /path/to/backup
```

## Completion

- [ ] All 40 tests attempted
- [ ] `zenith-hardware-probe` log collected
- [ ] Issues filed for any FAIL items

