# ZenithOS Hardware Profiles

Hardware profiles keep the development VM and final laptop target separate
without forking the OS image.

Selection order at boot:

1. Kernel command line: `zenith.profile=<id>`.
2. Installed override: `/etc/zenith/hardware-profile`.
3. Automatic detection from DMI, PCI, and CPU data.
4. Fallback: `generic-workstation`.

The detector writes:

```text
/run/zenith/hardware-profile
/run/zenith/hardware-profile.env
```

System services, Settings, diagnostics, and the installer should consume the
runtime files instead of hard-coding a target machine.
