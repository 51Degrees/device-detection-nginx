param (
    [string]$DeviceDetection,
    [string]$DeviceDetectionUrl,
    [Parameter(Mandatory)][string]$NginxKey,
    [Parameter(Mandatory)][string]$NginxCert,
    [Parameter(Mandatory)][string]$NginxJwtToken
)
$ErrorActionPreference = "Stop"

$assets = "TAC-HashV41.hash", "20000 Evidence Records.yml", "20000 User Agents.csv"
./steps/fetch-assets.ps1 -DeviceDetection:$DeviceDetection -DeviceDetectionUrl:$DeviceDetectionUrl -Assets $assets
foreach ($asset in $assets) {
    New-Item -ItemType SymbolicLink -Force -Target "$PWD/assets/$asset" -Path "$PSScriptRoot/../device-detection-cxx/device-detection-data/$asset"
}

# Write the key and certificate files for NGINX Plus so it can be installed.
Write-Host "Writing NGINX Plus repo key"
$NginxKey > "$PSScriptRoot/../nginx-repo.key"
Write-Host "Writing NGINX Plus repo certificate"
$NginxCert > "$PSScriptRoot/../nginx-repo.crt"
Write-Host "Writing NGINX Plus Jwt token"
$NginxJwtToken > "$PSScriptRoot/../license.jwt"
