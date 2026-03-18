param(
    [string]$Name = "1.21.3_dynamic",
    [string]$NginxVersion = "1.21.3",
    [string]$BuildMethod = "dynamic",
    [bool]$MemCheck = $False
)
$ErrorActionPreference = "Stop"
$PSNativeCommandUseErrorActionPreference = $true

Push-Location $PSScriptRoot/..
try {
    $build = $BuildMethod -eq 'static' ? 'STATIC_BUILD=1' : $null
    $target = $MemCheck ? 'mem-check' : 'install'

    Write-Host "Building module, target $target $build"
    make DONT_CLEAN_TESTS=1 FIFTYONEDEGREES_NGINX_VERSION=$NginxVersion FIFTYONEDEGREES_DATAFILE=TAC-HashV41.hash $build $target
} finally {
    Pop-Location
}
