param(
    [int]$DefaultSeconds = 75
)

$ErrorActionPreference = "Stop"

$repo = (Resolve-Path ".").Path
$outDir = Join-Path $repo "build/workstation/qemu-diagnostic"
$iso = Join-Path $repo "build/workstation/zenithos-seed.iso"
$kernel = Join-Path $repo "build/workstation/iso-stage/live/vmlinuz"
$initrd = Join-Path $repo "build/workstation/iso-stage/live/initrd.img"
$qemu = (Get-Command qemu-system-x86_64).Source

New-Item -ItemType Directory -Force -Path $outDir | Out-Null

if (-not (Test-Path $iso)) { throw "missing ISO: $iso" }
if (-not (Test-Path $kernel)) { throw "missing kernel: $kernel" }
if (-not (Test-Path $initrd)) { throw "missing initrd: $initrd" }

$append = "boot=live components username=zenith hostname=zenithos locales=en_US.UTF-8 keyboard-layouts=us console=tty0 console=ttyS0,115200n8 systemd.show_status=1 loglevel=4 zenith.boot_report=1 zenith.profile=dev-vm"

function Get-CompactTail {
    param(
        [string]$Path,
        [int]$Tail = 80,
        [int]$MaxLine = 500,
        [int]$MaxTotal = 20000
    )

    if (-not (Test-Path $Path)) {
        return @()
    }

    $total = 0
    $lines = @()
    foreach ($line in (Get-Content $Path -Tail $Tail)) {
        if ($line.Length -gt $MaxLine) {
            $line = $line.Substring(0, $MaxLine) + "...<truncated>"
        }
        $total += $line.Length
        if ($total -gt $MaxTotal) {
            $lines += "...<tail truncated>"
            break
        }
        $lines += $line
    }
    return $lines
}

$cases = @(
    @{
        Name = "none-virtio-tcg"
        Display = "none"
        Video = @("-vga", "virtio")
        Accel = @("-accel", "tcg,thread=multi")
        Seconds = $DefaultSeconds
    },
    @{
        Name = "gtk-virtio-tcg"
        Display = "gtk,gl=off"
        Video = @("-vga", "virtio")
        Accel = @("-accel", "tcg,thread=multi")
        Seconds = $DefaultSeconds
    },
    @{
        Name = "gtk-std-tcg"
        Display = "gtk,gl=off"
        Video = @("-vga", "std")
        Accel = @("-accel", "tcg,thread=multi")
        Seconds = $DefaultSeconds
    },
    @{
        Name = "gtk-qxl-tcg"
        Display = "gtk,gl=off"
        Video = @("-vga", "qxl")
        Accel = @("-accel", "tcg,thread=multi")
        Seconds = $DefaultSeconds
    },
    @{
        Name = "gtk-virtio-whpx"
        Display = "gtk,gl=off"
        Video = @("-vga", "virtio")
        Accel = @("-accel", "whpx")
        Seconds = $DefaultSeconds
    },
    @{
        Name = "sdl-virtio-tcg"
        Display = "sdl"
        Video = @("-vga", "virtio")
        Accel = @("-accel", "tcg,thread=multi")
        Seconds = 20
    }
)

$results = @()

foreach ($case in $cases) {
    $name = $case.Name
    $serial = Join-Path $outDir "$name-serial.log"
    $stderr = Join-Path $outDir "$name-stderr.log"
    Remove-Item -Force $serial, $stderr -ErrorAction SilentlyContinue

    $args = @(
        "-machine", "q35",
        "-m", "4096M",
        "-cpu", "max",
        "-smp", "2"
    ) + $case.Accel + $case.Video + @(
        "-device", "virtio-rng-pci",
        "-device", "qemu-xhci",
        "-device", "usb-tablet",
        "-cdrom", $iso,
        "-kernel", $kernel,
        "-initrd", $initrd,
        "-append", "`"$append`"",
        "-display", $case.Display,
        "-serial", "file:$serial",
        "-monitor", "none",
        "-no-reboot"
    )

    $startedAt = Get-Date
    $samples = @()
    $exitCode = $null
    $launchError = $null

    try {
        $process = Start-Process -FilePath $qemu -ArgumentList $args -RedirectStandardError $stderr -PassThru
        for ($elapsed = 5; $elapsed -le [int]$case.Seconds; $elapsed += 5) {
            Start-Sleep -Seconds 5
            $proc = Get-Process -Id $process.Id -ErrorAction SilentlyContinue
            if (-not $proc) {
                $samples += [pscustomobject]@{ second = $elapsed; state = "exited" }
                break
            }
            $samples += [pscustomobject]@{
                second = $elapsed
                responding = $proc.Responding
                cpu = [math]::Round($proc.CPU, 2)
                main_window = $proc.MainWindowTitle
            }
        }
        if ($process.HasExited) {
            $exitCode = $process.ExitCode
        } else {
            Stop-Process -Id $process.Id -Force
            Start-Sleep -Milliseconds 500
            $exitCode = "stopped-by-diagnostic"
        }
    } catch {
        $launchError = $_.Exception.Message
    }

    $stderrTail = Get-CompactTail -Path $stderr -Tail 80
    $serialTail = Get-CompactTail -Path $serial -Tail 120

    $results += [pscustomobject]@{
        name = $name
        display = $case.Display
        video = ($case.Video -join " ")
        accel = ($case.Accel -join " ")
        started_at = $startedAt.ToString("o")
        exit_code = $exitCode
        launch_error = $launchError
        samples = $samples
        stderr_tail = $stderrTail
        serial_tail = $serialTail
    }
}

$summaryPath = Join-Path $outDir "matrix-summary.json"
$textPath = Join-Path $outDir "matrix-summary.txt"
$results | ConvertTo-Json -Depth 6 | Set-Content -Encoding UTF8 $summaryPath

$textLines = foreach ($result in $results) {
    "=== $($result.name) ==="
    "display=$($result.display) video=$($result.video) accel=$($result.accel)"
    "exit=$($result.exit_code) launch_error=$($result.launch_error)"
    "samples:"
    foreach ($sample in $result.samples) {
        "  $($sample.second)s responding=$($sample.responding) cpu=$($sample.cpu) state=$($sample.state)"
    }
    "stderr_tail:"
    foreach ($line in $result.stderr_tail) { "  $line" }
    "serial_tail:"
    foreach ($line in ($result.serial_tail | Select-Object -Last 20)) { "  $line" }
    ""
}
$textLines | Set-Content -Encoding UTF8 $textPath

Write-Output "QEMU diagnostic matrix written to $summaryPath"
Write-Output "Text summary written to $textPath"
Get-Content $textPath
