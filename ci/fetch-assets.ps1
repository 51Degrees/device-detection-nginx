param (
    [Parameter(Mandatory=$true)]
    [string]$RepoName,
    [Parameter(Mandatory=$true)]
    [string]$DeviceDetection,
    [string]$DeviceDetectionUrl,
    [Parameter(Mandatory=$true)]
    [string]$NginxKey,
    [Parameter(Mandatory=$true)]
    [string]$NginxCert,
    [Parameter(Mandatory=$true)]
    [string]$NginxJwtToken
)

$RepoPath = [IO.Path]::Combine($pwd, $RepoName)

# Get the TAC data file for testing
Write-Output "Downloading Hash data file"
./steps/fetch-hash-assets.ps1 -RepoName $RepoName -LicenseKey $DeviceDetection -Url $DeviceDetectionUrl

# And move it to the correct directory
Write-Output "Moving Hash data file"
Move-Item $RepoPath/TAC-HashV41.hash  $RepoPath/device-detection-cxx/device-detection-data/TAC-HashV41.hash

# Write the key and certificate files for NGINX Plus so it can be installed.
Write-Output "Writing NGINX Plus repo key"
Write-Output $NginxKey >> $RepoPath/nginx-repo.key
Write-Output "Writing NGINX Plus repo certificate"
Write-Output $NginxCert >> $RepoPath/nginx-repo.crt
Write-Output "Writing NGINX Plus Jwt token"
Write-Output $NginxJwtToken >> $RepoPath/license.jwt

# Pull the evidence files for testing as they are not by default.
$DataFileDir = [IO.Path]::Combine($RepoPath, "device-detection-cxx", "device-detection-data")
Push-Location $DataFileDir
try {
    Write-Output "Pulling evidence files"
    git lfs pull -I "*.csv" 
    git lfs pull -I "*.yml"
}
finally {
    Pop-Location
}
