param(
    [int]$Seconds = 90,
    [switch]$Persistent,
    [switch]$InstalledDisk,
    [switch]$ManualLogin,
    [string]$ManualPassword = "2976880801"
)

$ErrorActionPreference = "Stop"

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$WorkstationDir = Join-Path $RepoRoot "build\workstation"
$SeedIso = Join-Path $WorkstationDir "zenithos-seed.iso"
$PersistenceDisk = Join-Path $WorkstationDir "zenithos-persistence.raw"
$InstallDisk = Join-Path $WorkstationDir "zenithos-install-target.qcow2"
$LiveKernel = Join-Path $WorkstationDir "iso-stage\live\vmlinuz"
$LiveInitrd = Join-Path $WorkstationDir "iso-stage\live\initrd.img"
$IsInstalledBoot = $InstalledDisk -or $ManualLogin
$SerialLog = Join-Path $WorkstationDir $(if ($ManualLogin) { "manual-login-smoke-serial.log" } elseif ($InstalledDisk) { "installed-smoke-serial.log" } else { "smoke-serial.log" })
$StderrLog = Join-Path $WorkstationDir $(if ($ManualLogin) { "manual-login-smoke-qemu-stderr.log" } elseif ($InstalledDisk) { "installed-smoke-qemu-stderr.log" } else { "smoke-qemu-stderr.log" })
$ManualInputLog = Join-Path $WorkstationDir "manual-login-smoke-input.log"
$OvmfCode = "C:\Program Files\qemu\share\edk2-x86_64-code.fd"
$OvmfVars = "C:\Program Files\qemu\share\edk2-i386-vars.fd"
$RunOvmf = Join-Path $WorkstationDir "zenithos-installed-smoke-code.fd"
$RunOvmfVars = Join-Path $WorkstationDir "zenithos-installed-smoke-vars.fd"

function Require-File($Path, $Hint) {
    if (-not (Test-Path $Path)) {
        throw "missing $Path; $Hint"
    }
}

function Quote-Arg($Value) {
    '"' + ($Value -replace '"', '\"') + '"'
}

function Write-ManualInputLog($Message) {
    if ($ManualLogin) {
        $timestamp = Get-Date -Format "yyyy-MM-ddTHH:mm:ss.fffK"
        Add-Content -Path $ManualInputLog -Value "[$timestamp] $Message"
    }
}

function Get-FreeTcpPort {
    $listener = [System.Net.Sockets.TcpListener]::new([System.Net.IPAddress]::Parse("127.0.0.1"), 0)
    $listener.Start()
    try {
        return $listener.LocalEndpoint.Port
    } finally {
        $listener.Stop()
    }
}

function Send-MonitorCommand($Port, $Command) {
    $client = [System.Net.Sockets.TcpClient]::new()
    $client.Connect("127.0.0.1", $Port)
    try {
        $stream = $client.GetStream()
        $stream.WriteTimeout = 1000
        $bytes = [System.Text.Encoding]::ASCII.GetBytes("$Command`n")
        $stream.Write($bytes, 0, $bytes.Length)
        $stream.Flush()
        $response = [System.Text.StringBuilder]::new()
        $buffer = New-Object byte[] 4096
        $until = (Get-Date).AddMilliseconds(1200)
        while ((Get-Date) -lt $until) {
            if ($stream.DataAvailable) {
                $read = $stream.Read($buffer, 0, $buffer.Length)
                if ($read -gt 0) {
                    [void]$response.Append([System.Text.Encoding]::ASCII.GetString($buffer, 0, $read))
                    $until = (Get-Date).AddMilliseconds(250)
                }
            } else {
                Start-Sleep -Milliseconds 50
            }
        }
        $text = $response.ToString().Trim()
        Write-ManualInputLog "monitor> $Command"
        if ($text.Length -gt 0) {
            Write-ManualInputLog ($text -replace "`r?`n", "`n")
        } else {
            Write-ManualInputLog "monitor< <no response>"
        }
        return $text
    } finally {
        $client.Close()
    }
}

function Send-MonitorKey($Port, $Key) {
    [void](Send-MonitorCommand $Port "sendkey $Key")
    Start-Sleep -Milliseconds 180
}

function Send-SddmPassword($Port, $Password) {
    foreach ($char in $Password.ToCharArray()) {
        switch ($char) {
            "0" { Send-MonitorKey $Port "0" }
            "1" { Send-MonitorKey $Port "1" }
            "2" { Send-MonitorKey $Port "2" }
            "3" { Send-MonitorKey $Port "3" }
            "4" { Send-MonitorKey $Port "4" }
            "5" { Send-MonitorKey $Port "5" }
            "6" { Send-MonitorKey $Port "6" }
            "7" { Send-MonitorKey $Port "7" }
            "8" { Send-MonitorKey $Port "8" }
            "9" { Send-MonitorKey $Port "9" }
            default { throw "manual-login smoke only supports numeric password characters" }
        }
    }
    Send-MonitorKey $Port "ret"
}

if ($IsInstalledBoot) {
    Require-File $InstallDisk "create and install it with mingw32-make workstation-vm-install-disk and workstation-run-seed-install-target first"
    Require-File $OvmfCode "install QEMU with OVMF firmware"
    Require-File $OvmfVars "install QEMU with OVMF firmware"
} else {
    Require-File $SeedIso "build it with mingw32-make workstation-seed-iso-wsl first"
    Require-File $LiveKernel "rebuild with mingw32-make workstation-seed-iso-wsl first"
    Require-File $LiveInitrd "rebuild with mingw32-make workstation-seed-iso-wsl first"
}
if ($Persistent -and -not $IsInstalledBoot) {
    Require-File $PersistenceDisk "create it with mingw32-make workstation-vm-persistence-wsl first"
}

New-Item -ItemType Directory -Force -Path $WorkstationDir | Out-Null
Remove-Item -Force $SerialLog, $StderrLog, $ManualInputLog -ErrorAction SilentlyContinue

$MonitorPort = $null
if ($ManualLogin) {
    $MonitorPort = Get-FreeTcpPort
}

if ($IsInstalledBoot) {
    Copy-Item -Force $OvmfCode $RunOvmf
    Copy-Item -Force $OvmfVars $RunOvmfVars
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
        $(if ($ManualLogin) { "-monitor tcp:127.0.0.1:$MonitorPort,server=on,wait=off" } else { "-monitor none" }),
        "-no-reboot",
        "-drive if=pflash,format=raw,readonly=on,file=$(Quote-Arg $RunOvmf)",
        "-drive if=pflash,format=raw,file=$(Quote-Arg $RunOvmfVars)",
        "-drive file=$(Quote-Arg $InstallDisk),if=virtio,format=qcow2,cache=writeback",
        "-boot order=c,strict=on"
    )
} else {
    $kernelArgs = "boot=live username=zenith hostname=zenithos locales=en_US.UTF-8 keyboard-layouts=us live-config.noautologin live-config.nox11autologin console=tty0 console=ttyS0,115200n8 systemd.show_status=1 loglevel=4 plymouth.enable=0 zenith.boot_report=1 zenith.profile=dev-vm"
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
}

$argumentString = $argsList -join " "
$process = Start-Process `
    -FilePath "qemu-system-x86_64" `
    -ArgumentList $argumentString `
    -WorkingDirectory $RepoRoot `
    -RedirectStandardError $StderrLog `
    -PassThru

$deadline = (Get-Date).AddSeconds($Seconds)
$manualLoginSent = $false
while ((Get-Date) -lt $deadline) {
    Start-Sleep -Seconds 2
    if ($ManualLogin -and -not $manualLoginSent) {
        $partialSerial = if (Test-Path $SerialLog) { Get-Content $SerialLog -Raw } else { "" }
        if ($partialSerial -match "(?i)(zenith-session-report-watch: waiting for|Started .*sddm\.service|sddm\.service - Simple Desktop Display Manager|Active:\s+active .*sddm)") {
            Write-ManualInputLog "manual-login trigger observed before session watcher timeout"
            Start-Sleep -Seconds 10
            Send-SddmPassword $MonitorPort $ManualPassword
            $manualLoginSent = $true
        }
    }
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

if (-not (Test-Path $SerialLog)) {
    throw "QEMU smoke test did not create a serial log. See $SerialLog"
}

$serialInfo = Get-Item $SerialLog
if ($serialInfo.Length -le 0) {
    throw "QEMU smoke test serial log is empty. See $SerialLog"
}

if (-not $IsInstalledBoot) {
    $referenceImage = Get-Item $SeedIso
    if ($serialInfo.LastWriteTime -lt $referenceImage.LastWriteTime) {
        throw "QEMU smoke test serial log is older than the boot image. Re-run smoke after rebuilding or reinstalling. See $SerialLog"
    }
}

if ($stderr -match "(?i)(could not|failed to|invalid|fatal|error)") {
    throw "QEMU smoke test stderr contains an error. See $StderrLog"
}

if ($serial -match "(?i)(kernel panic|panic|segmentation fault|emergency mode|failed to start)") {
    throw "QEMU smoke test serial log contains a boot failure. See $SerialLog"
}

if ($serial -notmatch "(?i)(linux version|systemd|zenithos|debian)") {
    throw "QEMU smoke test did not capture expected boot text. See $SerialLog"
}

if ($IsInstalledBoot) {
    if ($serial -match "(?i)(boot=live|live-config)") {
        throw "Installed-disk smoke captured live-boot arguments; this did not prove installed boot. See $SerialLog"
    }
    if ($serial -notmatch "(?i)(Reached target .*graphical\.target|Reached target .*Graphical Interface|graphical\.target)") {
        throw "Installed-disk smoke did not reach graphical.target. See $SerialLog"
    }
    if ($serial -notmatch "(?i)(Started .*sddm\.service|sddm\.service - Simple Desktop Display Manager|Active:\s+active .*sddm)") {
        throw "Installed-disk smoke did not prove SDDM is active. See $SerialLog"
    }
    $requiredSessionEvidence = @(
        "=== ZenithOS session report ===",
        "result: PASS",
        "plasmashell: running",
        "panel_package: present",
        "icontasks: present",
        "wallpaper: present"
    )
    foreach ($evidence in $requiredSessionEvidence) {
        if ($serial -notmatch [regex]::Escape($evidence)) {
            throw "Installed-disk smoke did not capture session evidence: $evidence. See $SerialLog"
        }
    }
    if ($ManualLogin) {
        if (-not $manualLoginSent) {
            throw "Manual-login smoke never sent SDDM credentials. See $SerialLog"
        }
        if (-not (Test-Path $ManualInputLog)) {
            throw "Manual-login smoke did not create monitor input evidence. See $ManualInputLog"
        }
        if ($serial -notmatch "zenith.manual_login_smoke=1") {
            throw "Manual-login smoke did not boot a manual-login smoke install. See $SerialLog"
        }
        if ($serial -match "zenith.desktop_smoke=1") {
            throw "Manual-login smoke boot still has desktop autologin smoke enabled. See $SerialLog"
        }
        if ($serial -match "zenith-desktop-smoke-login: enabled SDDM autologin") {
            throw "Manual-login smoke used the SDDM autologin gate. See $SerialLog"
        }
    }
    Write-Host "workstation-installed-smoke: ok"
    Write-Host "Serial log: $SerialLog"
    Write-Host "QEMU stderr: $StderrLog"
    if ($ManualLogin) {
        Write-Host "QEMU monitor input: $ManualInputLog"
    }
    exit 0
}

if ($serial -notmatch "(?i)(Reached target .*graphical\.target|Reached target .*Graphical Interface)") {
    throw "QEMU smoke test did not reach graphical.target. See $SerialLog"
}

if ($serial -notmatch "(?i)(Started .*sddm\.service|sddm\.service - Simple Desktop Display Manager|Active:\s+active .*sddm)") {
    throw "QEMU smoke test did not prove SDDM is active. See $SerialLog"
}

if ($serial -notmatch "=== ZenithOS boot report ===") {
    throw "QEMU smoke test did not capture the ZenithOS boot report. See $SerialLog"
}

if ($serial -notmatch "Current=pixel-skyscrapers") {
    throw "QEMU smoke test did not prove the pixel-skyscrapers SDDM theme is configured. See $SerialLog"
}

if ($serial -match 'The configured theme "debian-theme"') {
    throw "QEMU smoke test shows SDDM still selected the Debian fallback theme. See $SerialLog"
}

$requiredThemeAssets = @(
    "/usr/share/sddm/themes/pixel-skyscrapers/Main.qml",
    "/usr/share/sddm/themes/pixel-skyscrapers/BackgroundVideo.qml",
    "/usr/share/sddm/themes/pixel-skyscrapers/bg.mp4",
    "/usr/share/sddm/themes/pixel-skyscrapers/theme.conf",
    "/usr/share/sddm/themes/pixel-skyscrapers/font/PixelifySans-Bold.ttf"
)
foreach ($asset in $requiredThemeAssets) {
    if ($serial -notmatch [regex]::Escape("theme asset present: $asset")) {
        throw "QEMU smoke test did not prove SDDM theme asset is present: $asset. See $SerialLog"
    }
}

if ($serial -match "theme asset missing:") {
    throw "QEMU smoke test found a missing SDDM theme asset. See $SerialLog"
}

Write-Host "workstation-smoke: ok"
Write-Host "Serial log: $SerialLog"
Write-Host "QEMU stderr: $StderrLog"
