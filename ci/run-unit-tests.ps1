
param(
    [Parameter(Mandatory=$true)]
    [string]$RepoName,
    [string]$Name = "1.21.3_dynamic",
    [string]$NginxVersion = "1.21.3",
    [string]$BuildMethod = "dynamic",
    [bool]$FullTests = $False,
    [bool]$MemCheck = $False
)

$RepoPath = [IO.Path]::Combine($pwd, $RepoName)

Write-Output "Entering '$RepoPath'"
Push-Location $RepoPath

try {

    # Create the output directories if they don't exist already.
    if ($(Test-Path -Path "test-results") -eq  $False) {
        mkdir test-results
    }
    if ($(Test-Path -Path "test-results/unit") -eq  $False) {
        mkdir test-results/unit
    }

    Write-Output "Running 51Degrees unit tests"
    make test FIFTYONEDEGREES_DATAFILE=TAC-HashV41.hash FIFTYONEDEGREES_FORMATTER='--formatter TAP::Formatter::JUnit' FIFTYONEDEGREES_TEST_OUTPUT=$RepoPath/test-results/unit/$Name.xml

    if ($FullTests -eq $True) {
    
        Write-Output "Running full NGINX unit tests"
        make test-full FIFTYONEDEGREES_DATAFILE=TAC-HashV41.hash FIFTYONEDEGREES_FORMATTER='--formatter TAP::Formatter::JUnit' FIFTYONEDEGREES_TEST_OUTPUT=$RepoPath/test-results/unit/$($Name)_Full.xml
    
    }

}
finally {

    Write-Output "Leaving '$RepoPath'"
    Pop-Location

}
