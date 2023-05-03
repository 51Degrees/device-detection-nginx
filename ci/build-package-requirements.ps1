param(
    [Parameter(Mandatory=$true)]
    [string]$RepoName,
    [string]$Name = "1.21.3_dynamic",
    [string]$NginxVersion = "1.21.3"
)

$RepoPath = [IO.Path]::Combine($pwd, $RepoName)

Write-Output "Entering '$RepoPath'"
Push-Location $RepoPath

try {

    make intall FIFTYONEDEGREES_NGINX_VERSION=$NginxVersion
        
    # Create a directory for binary files from which they will be uploaded
    # as artifacts.
    New-Item -path "$RepoPath/package-files/$NginxVersion" -ItemType Directory -Force 
    
    $ModuleName = "ngx_http_51D_module.so"
    $InputFile = [IO.Path]::Combine($RepoPath, "build", "modules", $ModuleName)
    $OutputFile = [IO.Path]::Combine($RepoPath, "package-files", $NginxVersion, $ModuleName)

    # Copy module
    Copy-Item -Path $InputFile -Destination $OutputFile

}
finally {

    Write-Output "Leaving '$RepoPath'"
    Pop-Location

}