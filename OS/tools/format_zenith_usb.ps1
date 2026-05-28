param(
    [int]$DiskNumber = 1,
    [string]$ExpectedName = "PNY USB 2.0 FD",
    [string]$DriveLetter = "D",
    [string]$Label = "ZENITHUSB",
    [string]$LogPath = "$PSScriptRoot\..\build\format-zenith-usb.log"
)

$ErrorActionPreference = "Stop"

New-Item -ItemType Directory -Force -Path (Split-Path -Parent $LogPath) | Out-Null
Start-Transcript -Path $LogPath -Force | Out-Null

try {
$disk = Get-Disk -Number $DiskNumber
if ($disk.BusType -ne "USB") {
    throw "Refusing to format: Disk $DiskNumber is not USB. BusType=$($disk.BusType)"
}
if ($disk.FriendlyName -ne $ExpectedName) {
    throw "Refusing to format: expected '$ExpectedName', got '$($disk.FriendlyName)'"
}
if ($disk.Size -lt 30000000000 -or $disk.Size -gt 33000000000) {
    throw "Refusing to format: unexpected size $($disk.Size)"
}
if ($disk.IsBoot -or $disk.IsSystem) {
    throw "Refusing to format: disk is marked boot/system."
}

if ($disk.IsReadOnly) {
    Set-Disk -Number $disk.Number -IsReadOnly $false
}
if ($disk.OperationalStatus -notcontains "Online") {
    Set-Disk -Number $disk.Number -IsOffline $false
}

Get-Partition -DiskNumber $disk.Number -ErrorAction SilentlyContinue |
    Remove-Partition -Confirm:$false -ErrorAction SilentlyContinue

Clear-Disk -Number $disk.Number -RemoveData -RemoveOEM -Confirm:$false
$disk = Get-Disk -Number $disk.Number
if ($disk.PartitionStyle -eq "RAW") {
    Initialize-Disk -Number $disk.Number -PartitionStyle MBR
}
$partition = New-Partition -DiskNumber $disk.Number -UseMaximumSize -DriveLetter $DriveLetter
$partition | Format-Volume -FileSystem exFAT -NewFileSystemLabel $Label -Confirm:$false

Get-Volume -DriveLetter $DriveLetter |
    Select-Object DriveLetter, FileSystemLabel, FileSystem, DriveType, HealthStatus, Size, SizeRemaining |
    Format-List
}
finally {
    Stop-Transcript | Out-Null
}
