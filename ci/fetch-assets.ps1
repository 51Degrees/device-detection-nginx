param (
    [string]$DeviceDetection,
    [string]$DeviceDetectionUrl
)
$ErrorActionPreference = "Stop"

$assets = "TAC-HashV41.hash", "20000 Evidence Records.yml", "20000 User Agents.csv"
./steps/fetch-assets.ps1 -DeviceDetection:$DeviceDetection -DeviceDetectionUrl:$DeviceDetectionUrl -Assets $assets
foreach ($asset in $assets) {
    New-Item -ItemType SymbolicLink -Force -Target "$PWD/assets/$asset" -Path "$PSScriptRoot/../device-detection-cxx/device-detection-data/$asset"
}

Push-Location "$PSScriptRoot/../ip-intelligence-cxx/ip-intelligence-data"
try {
    Write-Host "Entering $PWD"
    # Remove old Asn file (if exists)
    $AsnFilePath = "51Degrees-IPIV4AsnIpiV41.ipi"
    if (Test-Path -Type Leaf -Path $AsnFilePath) {
        Remove-Item -Path $AsnFilePath
        Write-Host "Deleted $AsnFilePath"
    }
    
    Write-Host "Loading free IPI data files..."
    & ./get-lite-file-from-azure.ps1
} finally {
    Write-Host "Leaving $PWD"
    Pop-Location
}
