param(
    [Parameter(Mandatory=$true)]
    [string]$RepoName,
    [string]$Name,
    [string]$NginxVersion
)

$RepoPath = [IO.Path]::Combine($pwd, $RepoName)

Write-Output "Entering '$RepoPath'"
Push-Location $RepoPath

try {

    make install FIFTYONEDEGREES_NGINX_VERSION=$NginxVersion
        
    # Create a directory for binary files from which they will be uploaded
    # as artifacts.
    New-Item -path "$RepoPath/package-files" -ItemType Directory -Force 
    
    $ModuleName = "ngx_http_51D_module.so"
    $InputFile = [IO.Path]::Combine($RepoPath, "build", "modules", $ModuleName)
    $OutputFile = [IO.Path]::Combine($RepoPath, "package-files", $ModuleName)

    # Copy module
    Copy-Item -Path $InputFile -Destination $OutputFile

}
finally {

    Write-Output "Leaving '$RepoPath'"
    Pop-Location

}
