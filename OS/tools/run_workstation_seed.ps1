param(
    [ValidateSet("direct", "splash", "iso", "fast", "persistent", "install-target", "autoinstall", "autoinstall-manual-login", "installed")]
    [string]$Mode = "direct",
    [switch]$Wait,
    [switch]$DryRun
)

$ErrorActionPreference = "Stop"

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$BuildDir = Join-Path $RepoRoot "build"
$WorkstationDir = Join-Path $BuildDir "workstation"
$SeedIso = Join-Path $WorkstationDir "zenithos-seed.iso"
$PersistenceDisk = Join-Path $WorkstationDir "zenithos-persistence.raw"
$InstallDisk = Join-Path $WorkstationDir "zenithos-install-target.qcow2"
$LiveKernel = Join-Path $WorkstationDir "iso-stage\live\vmlinuz"
$LiveInitrd = Join-Path $WorkstationDir "iso-stage\live\initrd.img"
$OvmfCode = "C:\Program Files\qemu\share\edk2-x86_64-code.fd"
$OvmfVars = "C:\Program Files\qemu\share\edk2-i386-vars.fd"
$RunOvmf = Join-Path $BuildDir "edk2-x86_64-code.fd"
$RunOvmfVars = Join-Path $BuildDir "edk2-vars.fd"
$InstallOvmfVars = Join-Path $WorkstationDir "zenithos-install-vars.fd"
$InstalledOvmfVars = Join-Path $WorkstationDir "zenithos-installed-vars.fd"

$LiveArgs = "boot=live username=zenith hostname=zenithos locales=en_US.UTF-8 keyboard-layouts=us live-config.noautologin live-config.nox11autologin console=tty0 console=ttyS0,115200n8 systemd.show_status=1 loglevel=4 plymouth.enable=0 zenith.boot_report=1 zenith.profile=dev-vm"
$SplashArgs = "boot=live username=zenith hostname=zenithos locales=en_US.UTF-8 keyboard-layouts=us live-config.noautologin live-config.nox11autologin quiet splash loglevel=3 systemd.show_status=0 plymouth.ignore-serial-consoles zenith.profile=dev-vm"

function Require-File($Path, $Hint) {
    if (-not (Test-Path $Path)) {
        throw "missing $Path; $Hint"
    }
}

function Quote-Arg($Value) {
    '"' + ($Value -replace '"', '\"') + '"'
}

Require-File $SeedIso "build it with mingw32-make workstation-seed-iso-wsl first"
New-Item -ItemType Directory -Force -Path $WorkstationDir | Out-Null

$SerialLog = Join-Path $WorkstationDir "seed-serial.log"
$AutoinstallSerialLog = Join-Path $WorkstationDir $(if ($Mode -eq "autoinstall-manual-login") { "manual-login-autoinstall-serial.log" } else { "autoinstall-serial.log" })
$StderrLog = Join-Path $WorkstationDir "qemu-stderr.log"
Remove-Item -Force $SerialLog, $AutoinstallSerialLog, $StderrLog -ErrorAction SilentlyContinue
"Detached QEMU launch: stderr is not captured unless tools/run_workstation_seed.ps1 is run with -Wait." | Set-Content -Path $StderrLog -Encoding ascii

$argsList = @(
    "-machine q35",
    "-m 4096M",
    "-smp 2",
    "-vga virtio",
    "-device virtio-rng-pci",
    "-device qemu-xhci",
    "-device usb-tablet",
    "-display gtk,gl=off",
    "-serial file:$(Quote-Arg $SerialLog)",
    "-monitor none"
)

if ($Mode -eq "fast") {
    Require-File $OvmfCode "install QEMU with OVMF firmware"
    Require-File $OvmfVars "install QEMU with OVMF firmware"
    Copy-Item -Force $OvmfCode $RunOvmf
    Copy-Item -Force $OvmfVars $RunOvmfVars
    $argsList += @(
        "-cpu qemu64",
        "-accel whpx",
        "-drive if=pflash,format=raw,readonly=on,file=$(Quote-Arg $RunOvmf)",
        "-drive if=pflash,format=raw,file=$(Quote-Arg $RunOvmfVars)",
        "-cdrom $(Quote-Arg $SeedIso)",
        "-boot d"
    )
} elseif ($Mode -eq "install-target") {
    Require-File $OvmfCode "install QEMU with OVMF firmware"
    Require-File $OvmfVars "install QEMU with OVMF firmware"
    Require-File $InstallDisk "create it with mingw32-make workstation-vm-install-disk first"
    Copy-Item -Force $OvmfCode $RunOvmf
    Copy-Item -Force $OvmfVars $InstallOvmfVars
    $argsList += @(
        "-cpu max",
        "-accel tcg,thread=multi",
        "-drive if=pflash,format=raw,readonly=on,file=$(Quote-Arg $RunOvmf)",
        "-drive if=pflash,format=raw,file=$(Quote-Arg $InstallOvmfVars)",
        "-drive file=$(Quote-Arg $InstallDisk),if=virtio,format=qcow2,cache=writeback",
        "-cdrom $(Quote-Arg $SeedIso)",
        "-boot d"
    )
} elseif ($Mode -eq "autoinstall" -or $Mode -eq "autoinstall-manual-login") {
    Require-File $LiveKernel "rebuild with mingw32-make workstation-seed-iso-wsl first"
    Require-File $LiveInitrd "rebuild with mingw32-make workstation-seed-iso-wsl first"
    Require-File $InstallDisk "create it with mingw32-make workstation-vm-install-disk first"
    $smokeArg = if ($Mode -eq "autoinstall-manual-login") { "zenith.autoinstall_manual_login_smoke=1" } else { "zenith.autoinstall_desktop_smoke=1" }
    $kernelArgs = "$LiveArgs zenith.autoinstall_target=/dev/vda zenith.autoinstall_poweroff=1 $smokeArg systemd.unit=multi-user.target"
    $argsList = @(
        "-machine q35",
        "-m 4096M",
        "-smp 2",
        "-cpu qemu64",
        "-accel whpx",
        "-vga virtio",
        "-device virtio-rng-pci",
        "-device qemu-xhci",
        "-device usb-tablet",
        "-display none",
        "-serial file:$(Quote-Arg $AutoinstallSerialLog)",
        "-monitor none",
        "-no-reboot",
        "-drive file=$(Quote-Arg $InstallDisk),if=virtio,format=qcow2,cache=writeback",
        "-cdrom $(Quote-Arg $SeedIso)",
        "-kernel $(Quote-Arg $LiveKernel)",
        "-initrd $(Quote-Arg $LiveInitrd)",
        "-append $(Quote-Arg $kernelArgs)"
    )
} elseif ($Mode -eq "installed") {
    Require-File $OvmfCode "install QEMU with OVMF firmware"
    Require-File $OvmfVars "install QEMU with OVMF firmware"
    Require-File $InstallDisk "create and install to it with mingw32-make workstation-run-seed-install-target first"
    Copy-Item -Force $OvmfCode $RunOvmf
    Copy-Item -Force $OvmfVars $InstalledOvmfVars
    $argsList += @(
        "-cpu max",
        "-accel tcg,thread=multi",
        "-drive if=pflash,format=raw,readonly=on,file=$(Quote-Arg $RunOvmf)",
        "-drive if=pflash,format=raw,file=$(Quote-Arg $InstalledOvmfVars)",
        "-drive file=$(Quote-Arg $InstallDisk),if=virtio,format=qcow2,cache=writeback",
        "-boot order=c,strict=on"
    )
} elseif ($Mode -eq "iso") {
    Require-File $OvmfCode "install QEMU with OVMF firmware"
    Require-File $OvmfVars "install QEMU with OVMF firmware"
    Copy-Item -Force $OvmfCode $RunOvmf
    Copy-Item -Force $OvmfVars $RunOvmfVars
    $argsList += @(
        "-cpu max",
        "-accel tcg,thread=multi",
        "-drive if=pflash,format=raw,readonly=on,file=$(Quote-Arg $RunOvmf)",
        "-drive if=pflash,format=raw,file=$(Quote-Arg $RunOvmfVars)",
        "-cdrom $(Quote-Arg $SeedIso)",
        "-boot d"
    )
} else {
    Require-File $LiveKernel "rebuild with mingw32-make workstation-seed-iso-wsl first"
    Require-File $LiveInitrd "rebuild with mingw32-make workstation-seed-iso-wsl first"
    $kernelArgs = if ($Mode -eq "splash") { $SplashArgs } else { $LiveArgs }
    if ($Mode -eq "persistent") {
        Require-File $PersistenceDisk "create it with mingw32-make workstation-vm-persistence-wsl first"
        $kernelArgs = "$LiveArgs persistence"
        $argsList += @(
            "-drive file=$(Quote-Arg $PersistenceDisk),if=virtio,format=raw,cache=writeback"
        )
    }
    $argsList += @(
        "-cpu qemu64",
        "-accel whpx",
        "-cdrom $(Quote-Arg $SeedIso)",
        "-kernel $(Quote-Arg $LiveKernel)",
        "-initrd $(Quote-Arg $LiveInitrd)",
        "-append $(Quote-Arg $kernelArgs)"
    )
}

$argumentString = $argsList -join " "
$startInfo = @{
    FilePath = "qemu-system-x86_64"
    ArgumentList = $argumentString
    WorkingDirectory = $RepoRoot
    PassThru = $true
}

if ($Wait) {
    $startInfo["Wait"] = $true
    $startInfo["RedirectStandardError"] = $StderrLog
}

if ($DryRun) {
    Write-Host "qemu-system-x86_64 $argumentString"
    exit 0
}

$process = Start-Process @startInfo
Write-Host "ZenithOS QEMU started: PID $($process.Id)"
Write-Host "Serial log: $SerialLog"
Write-Host "QEMU stderr: $StderrLog"
if (-not $Wait) {
    Write-Host "The VM is running in its own window; close that window to stop it."
}
