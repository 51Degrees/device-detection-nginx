
param(
    [Parameter(Mandatory=$true)]
    [string]$RepoName,
    [string]$Name = "",
    [string]$NginxVersion = ""
)

# Combine the current working directory with the repository name
$RepoPath = [IO.Path]::Combine($pwd, $RepoName)

$ModuleName = "ngx_http_51D_module.so"

# Define the path for downloaded artifacts
$PackagePath = [IO.Path]::Combine($pwd, "package", "package_$Name", $ModuleName)

# Get the local install path
$ModulesPath = [IO.Path]::Combine($RepoPath, "build", "modules")
$InstallPath = [IO.Path]::Combine($ModulesPath, $ModuleName)

# Display the repository path and enter it
Write-Output "Entering '$RepoPath'"
Push-Location $RepoPath

try {

    if ($Name -ne "") {

        # Install NGINX without building the module.
        Write-Output "Install NGINX '$NginxVersion' without 51Degrees module"
        make install-no-module FIFTYONEDEGREES_NGINX_VERSION=$NginxVersion

        # Now copy the module to the installs module directory
        Write-Output "Copying the 51Degrees module from '$PackagePath' to '$InstallPath'"
        mkdir $ModulesPath
        Copy-Item -Path $PackagePath -Destination $InstallPath

    }
    else {
    
        Write-Output "Not installing locally, as no NGINX version is specified."
        
    }
}
finally {
    
    # Leave the repository path and display it
    Write-Output "Leaving '$RepoPath'"
    Pop-Location

}

# Exit the script with the last exit code
exit $LASTEXITCODE
