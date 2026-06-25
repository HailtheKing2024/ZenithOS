param(
    [string]$Path,
    [string]$Size = "40G",
    [switch]$Force,
    [switch]$DryRun
)

$ErrorActionPreference = "Stop"

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$WorkstationDir = Join-Path $RepoRoot "build\workstation"
$DefaultPath = Join-Path $WorkstationDir "zenithos-install-target.qcow2"

if (-not $Path) {
    $Path = $DefaultPath
}

function Resolve-TargetPath($Value) {
    if ($Value -match '^(\\\\\.\\|/dev/)') {
        throw "refusing device path: $Value"
    }

    $parent = Split-Path -Parent $Value
    if (-not $parent) {
        $parent = $WorkstationDir
        $Value = Join-Path $parent $Value
    }

    New-Item -ItemType Directory -Force -Path $parent | Out-Null
    $resolvedParent = (Resolve-Path $parent).Path
    $target = Join-Path $resolvedParent (Split-Path -Leaf $Value)
    $allowedRoot = (Resolve-Path $WorkstationDir).Path

    if (-not $target.StartsWith($allowedRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "install disk must stay under $allowedRoot"
    }

    return $target
}

function Quote-Arg($Value) {
    '"' + ($Value -replace '"', '\"') + '"'
}

New-Item -ItemType Directory -Force -Path $WorkstationDir | Out-Null
$Target = Resolve-TargetPath $Path

if ((Test-Path $Target) -and -not $Force) {
    throw "install disk already exists: $Target; pass -Force to recreate it"
}

if (-not (Get-Command "qemu-img" -ErrorAction SilentlyContinue)) {
    throw "missing qemu-img; install QEMU and ensure qemu-img is on PATH"
}

$command = "qemu-img create -f qcow2 $(Quote-Arg $Target) $Size"
if ($DryRun) {
    Write-Host $command
    exit 0
}

if (Test-Path $Target) {
    Remove-Item -Force $Target
}

& qemu-img create -f qcow2 $Target $Size | Write-Host
Write-Host "ZenithOS install target disk created: $Target"
