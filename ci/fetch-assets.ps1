param (
    [Parameter(Mandatory=$true)]
    [string]$RepoName,
    [Parameter(Mandatory=$true)]
    [string]$DeviceDetection,
    [string]$DeviceDetectionUrl,
    [Parameter(Mandatory=$true)]
    [string]$NginxKey,
    [Parameter(Mandatory=$true)]
    [string]$NginxCert
)

$RepoPath = [IO.Path]::Combine($pwd, $RepoName)

Write-Output "Downloading Hash data file"
./steps/fetch-hash-assets.ps1 -RepoName $RepoName -LicenseKey $DeviceDetection -Url $DeviceDetectionUrl

Write-Output "Moving Hash data file"
Move-Item $RepoPath/TAC-HashV41.hash  $RepoPath/device-detection-cxx/device-detection-data/TAC-HashV41.hash

Write-Output "Writing NGINX Plus repo key"
Write-Output $NginxKey >> $RepoPath/nginx-repo.key

Write-Output "Writing NGINX Plus repo certificate"
Write-Output $NginxCert >> $RepoPath/nginx-repo.crt
