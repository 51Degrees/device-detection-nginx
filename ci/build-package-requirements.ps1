param(
    [Parameter(Mandatory=$true)]
    [string]$RepoName,
    [string]$Name,
    [string]$NginxVersion
)

$ModuleName = "ngx_http_51D_module.so"
$RepoPath = [IO.Path]::Combine($pwd, $RepoName)
$OutputDir = [IO.Path]::Combine($pwd, "package-files")
$OutputFile = [IO.Path]::Combine($OutputDir, $ModuleName)

Write-Output "Entering '$RepoPath'"
Push-Location $RepoPath

try {

    make install FIFTYONEDEGREES_NGINX_VERSION=$NginxVersion
        
    # Create a directory for binary files from which they will be uploaded
    # as artifacts.
    New-Item -Path $OutputDir -ItemType Directory -Force 
    
    $InputFile = [IO.Path]::Combine($RepoPath, "build", "modules", $ModuleName)

    # Copy module
    Copy-Item -Path $InputFile -Destination $OutputFile

}
finally {

    Write-Output "Leaving '$RepoPath'"
    Pop-Location

}
