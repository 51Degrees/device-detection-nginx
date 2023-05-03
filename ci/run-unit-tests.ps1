
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
    $Passed = $($LASTEXITCODE -eq 0)
    cat $RepoPath/test-results/unit/$Name.xml

    if ($FullTests -eq $True) {

        Write-Output "Running full NGINX unit tests"
        make test-full FIFTYONEDEGREES_DATAFILE=TAC-HashV41.hash FIFTYONEDEGREES_FORMATTER='--formatter TAP::Formatter::JUnit' FIFTYONEDEGREES_TEST_OUTPUT=$RepoPath/test-results/unit/$($Name)_Full.xml
        $Passed = $($Passed -and $LASTEXITCODE -eq 0)
        
        Write-Output "Running full NGINX unit tests against NGINX Plus"
        $env:TEST_NGINX_BINARY="/usr/sbin/nginx"
        $env:TEST_NGINX_GLOBALS="load_module $RepoPath/build/modules/ngx_http_51D_module.so;"
        $env:TEST_NGINX_GLOBALS_HTTP="51D_file_path $RepoPath/device-detection-cxx/device-detection-data/TAC-HashV41.hash;"
        prove --formatter TAP::Formatter::JUnit -v tests/51degrees.t tests/nginx-tests :: TAC-HashV41.hash > $RepoPath/test-results/unit/$($Name)_Full_Plus.xml
        $Passed = $($Passed -and $LASTEXITCODE -eq 0)
    }

}
finally {

    Write-Output "Leaving '$RepoPath'"
    Pop-Location

}

if ($Passed) {
    exit 0
}
else {
    exit 1
}
