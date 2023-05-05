
param (
    [Parameter(Mandatory=$true)]
    [string]$RepoName,
    [Parameter(Mandatory=$true)]
    [string]$VariableName
)

# This is a common step, so let's call that
./steps/get-next-package-version.ps1 -RepoName $RepoName -VariableName "GitVersion"

# Set the variable for the caller
Write-Output "Setting '$VariableName' to '$($GitVersion.SemVer)'"
Set-Variable -Name $VariableName -Value $GitVersion.SemVer -Scope Global

exit $LASTEXITCODE
