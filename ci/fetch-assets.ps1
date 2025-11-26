param (
    [Parameter(Mandatory)][string]$RepoName,
    [string]$DeviceDetection,
    [string]$DeviceDetectionUrl,
    [Parameter(Mandatory)][string]$NginxKey,
    [Parameter(Mandatory)][string]$NginxCert,
    [Parameter(Mandatory)][string]$NginxJwtToken
)
$ErrorActionPreference = "Stop"

$deviceDetectionData = "$RepoName/device-detection-cxx/device-detection-data"

./steps/fetch-assets.ps1 -DeviceDetection:$DeviceDetection -DeviceDetectionUrl:$DeviceDetectionUrl `
    -Assets "TAC-HashV41.hash", "20000 Evidence Records.yml", "20000 User Agents.csv"

New-Item -ItemType SymbolicLink -Force -Target "$PWD/assets/TAC-HashV41.hash" -Path "$deviceDetectionData/TAC-HashV41.hash"
New-Item -ItemType SymbolicLink -Force -Target "$PWD/assets/20000 Evidence Records.yml" -Path "$deviceDetectionData/20000 Evidence Records.yml"
New-Item -ItemType SymbolicLink -Force -Target "$PWD/assets/20000 User Agents.csv" -Path "$deviceDetectionData/20000 User Agents.csv"

# Write the key and certificate files for NGINX Plus so it can be installed.
Write-Host "Writing NGINX Plus repo key"
Write-Output $NginxKey > "$RepoName/nginx-repo.key"
Write-Host "Writing NGINX Plus repo certificate"
Write-Output $NginxCert > "$RepoName/nginx-repo.crt"
Write-Host "Writing NGINX Plus Jwt token"
Write-Output $NginxJwtToken > "$RepoName/license.jwt"
