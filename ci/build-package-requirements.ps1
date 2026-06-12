param(
    [Parameter(Mandatory=$true)]
    [string]$RepoName,
    [string]$Name,
    [string]$NginxVersion
)

$ModuleNames = @("ngx_http_51D_module.so", "ngx_http_51D_ipi_module.so")
$RepoPath = [IO.Path]::Combine($pwd, $RepoName)
$OutputDir = [IO.Path]::Combine($pwd, "package-files")

Write-Output "Entering '$RepoPath'"
Push-Location $RepoPath

try {

    make install FIFTYONEDEGREES_NGINX_VERSION=$NginxVersion
        
    # Create a directory for binary files from which they will be uploaded
    # as artifacts.
    New-Item -Path $OutputDir -ItemType Directory -Force 
    
    # Copy both the device detection and IP intelligence modules
    foreach ($ModuleName in $ModuleNames) {
        $InputFile = [IO.Path]::Combine($RepoPath, "build", "modules", $ModuleName)
        $OutputFile = [IO.Path]::Combine($OutputDir, $ModuleName)
        Copy-Item -Path $InputFile -Destination $OutputFile
    }

}
finally {

    Write-Output "Leaving '$RepoPath'"
    Pop-Location

}
