
param (
    [Parameter(Mandatory=$true)]
    [string]$VariableName
)

./steps/get-next-package-version.ps1 -RepoName "device-detection-nginx-test" -VariableName "GitVersion"

Write-Output "Setting '$VariableName' to '$($GitVersion.SemVer)'"
Set-Variable -Name $VariableName -Value $GitVersion.SemVer -Scope Global

exit $LASTEXITCODE
