param(
    [Parameter(Mandatory=$true)]
    [string]$RepoName,
    [string]$Name = "1.21.3_dynamic",
    [string]$NginxVersion = "1.21.3",
    [string]$BuildMethod = "dynamic",
    [bool]$MemCheck = $False
)

$RepoPath = [IO.Path]::Combine($pwd, $RepoName)

Write-Output "Entering '$RepoPath'"
Push-Location $RepoPath

try {

    $InstallCommand = "install"
    $BuildArgs = @("FIFTYONEDEGREES_NGINX_VERSION=$NginxVersion", "FIFTYONEDEGREES_DATAFILE=TAC-HashV41.hash")

    if ($BuildType -eq "static") {
        Write-Output "Configuring build for a static module"
        $BuildArgs += "STATIC_BUILD=1"
    }

    if ($MemCheck -eq $True) {
        Write-Output "Configuring build memomry checks"
        $InstallCommand = "mem-check"
    }

    Write-Output "Building module"
    & make $InstallCommand $BuildArgs

}
finally {

    Write-Output "Leaving '$RepoPath'"
    Pop-Location

}