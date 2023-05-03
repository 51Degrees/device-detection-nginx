
param(
    [Parameter(Mandatory=$true)]
    [string]$RepoName,
    [string]$Name,
    [string]$NginxVersion
)

# Combine the current working directory with the repository name
$RepoPath = [IO.Path]::Combine($pwd, $RepoName)

# Define the path for downloaded artifacts
$PackagePath = [IO.Path]::Combine($RepoPath, "package", "package_$Name")

# Get the local install path
$InstallPath = [IO.Path]::Combine($RepoPath, "build", "modules")

# Display the repository path and enter it
Write-Output "Entering '$RepoPath'"
Push-Location $RepoPath

try {

    Write-Output "Install NGINX '$NginxVersion' without 51Degrees module"
    make install-no-module FIFTYONEDEGREES_NGINX_VERSION=$NginxVersion

    Write-Output "Copying the 51Degrees module"
    Copy-Item -Path $PackagePath -Destination $InstallPath

}
finally {
    
    # Leave the repository path and display it
    Write-Output "Leaving '$RepoPath'"
    Pop-Location

}

# Exit the script with the last exit code
exit $LASTEXITCODE
