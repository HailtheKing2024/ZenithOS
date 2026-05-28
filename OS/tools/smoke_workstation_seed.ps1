param(
    [int]$Seconds = 90,
    [switch]$Persistent
)

$ErrorActionPreference = "Stop"

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$WorkstationDir = Join-Path $RepoRoot "build\workstation"
$SeedIso = Join-Path $WorkstationDir "zenithos-seed.iso"
$PersistenceDisk = Join-Path $WorkstationDir "zenithos-persistence.raw"
$LiveKernel = Join-Path $WorkstationDir "iso-stage\live\vmlinuz"
$LiveInitrd = Join-Path $WorkstationDir "iso-stage\live\initrd.img"
$SerialLog = Join-Path $WorkstationDir "smoke-serial.log"
$StderrLog = Join-Path $WorkstationDir "smoke-qemu-stderr.log"

function Require-File($Path, $Hint) {
    if (-not (Test-Path $Path)) {
        throw "missing $Path; $Hint"
    }
}

function Quote-Arg($Value) {
    '"' + ($Value -replace '"', '\"') + '"'
}

Require-File $SeedIso "build it with mingw32-make workstation-seed-iso-wsl first"
Require-File $LiveKernel "rebuild with mingw32-make workstation-seed-iso-wsl first"
Require-File $LiveInitrd "rebuild with mingw32-make workstation-seed-iso-wsl first"
if ($Persistent) {
    Require-File $PersistenceDisk "create it with mingw32-make workstation-vm-persistence-wsl first"
}

New-Item -ItemType Directory -Force -Path $WorkstationDir | Out-Null
Remove-Item -Force $SerialLog, $StderrLog -ErrorAction SilentlyContinue

$kernelArgs = "boot=live username=zenith hostname=zenithos locales=en_US.UTF-8 keyboard-layouts=us console=tty0 console=ttyS0,115200n8 systemd.show_status=1 loglevel=4 plymouth.enable=0 zenith.boot_report=1 zenith.profile=dev-vm"
if ($Persistent) {
    $kernelArgs = "$kernelArgs persistence"
}

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
    "-serial file:$(Quote-Arg $SerialLog)",
    "-monitor none",
    "-no-reboot",
    "-cdrom $(Quote-Arg $SeedIso)",
    "-kernel $(Quote-Arg $LiveKernel)",
    "-initrd $(Quote-Arg $LiveInitrd)",
    "-append $(Quote-Arg $kernelArgs)"
)

if ($Persistent) {
    $argsList = @(
        "-drive file=$(Quote-Arg $PersistenceDisk),if=virtio,format=raw,cache=writeback"
    ) + $argsList
}

$argumentString = $argsList -join " "
$process = Start-Process `
    -FilePath "qemu-system-x86_64" `
    -ArgumentList $argumentString `
    -WorkingDirectory $RepoRoot `
    -RedirectStandardError $StderrLog `
    -PassThru

$deadline = (Get-Date).AddSeconds($Seconds)
while ((Get-Date) -lt $deadline) {
    Start-Sleep -Seconds 2
    if ($process.HasExited) {
        break
    }
}

if (-not $process.HasExited) {
    Stop-Process -Id $process.Id -Force
    Start-Sleep -Milliseconds 300
}

$serial = if (Test-Path $SerialLog) { Get-Content $SerialLog -Raw } else { "" }
$stderr = if (Test-Path $StderrLog) { Get-Content $StderrLog -Raw } else { "" }

if ($stderr -match "(?i)(could not|failed to|invalid|fatal|error)") {
    throw "QEMU smoke test stderr contains an error. See $StderrLog"
}

if ($serial -match "(?i)(kernel panic|panic|segmentation fault|emergency mode|failed to start)") {
    throw "QEMU smoke test serial log contains a boot failure. See $SerialLog"
}

if ($serial -notmatch "(?i)(linux version|systemd|zenithos|debian)") {
    throw "QEMU smoke test did not capture expected boot text. See $SerialLog"
}

Write-Host "workstation-smoke: ok"
Write-Host "Serial log: $SerialLog"
Write-Host "QEMU stderr: $StderrLog"
