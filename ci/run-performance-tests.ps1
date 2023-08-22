
param(
    [Parameter(Mandatory=$true)]
    [string]$RepoName,
    [string]$Name = "1.21.3_dynamic",
    [string]$NginxVersion = "1.21.3",
    [string]$BuildMethod = "dynamic"
)

$RepoPath = [IO.Path]::Combine($pwd, $RepoName)
$PerfPath = [IO.Path]::Combine($RepoPath, "tests", "performance")
$PerfResultsFile = [IO.Path]::Combine($RepoPath, "test-results", "performance-summary", "results_$Name.json")

Write-Output "Entering '$RepoPath'"
Push-Location $RepoPath

try {

    # Create the output directories if they don't already exist.
    if ($(Test-Path -Path "test-results") -eq  $False) {
        mkdir test-results
    }
    if ($(Test-Path -Path "test-results/performance") -eq  $False) {
        mkdir test-results/performance
    }
    if ($(Test-Path -Path "test-results/performance-summary") -eq  $False) {
        mkdir test-results/performance-summary
    }

}
finally {

    Write-Output "Leaving '$RepoPath'"
    Pop-Location

}

Write-Output "Entering '$PerfPath'"
Push-Location $PerfPath

try {
    mkdir build
    Push-Location build
    try {

        # Build the performance tests
        Write-Output "Building performance test"
        cmake ..
        cmake --build .

        # When running the performance tests, set the data file name manually,
        # then unset once we're done
        Write-Output "Running performance test"
        $env:DATA_FILE_NAME="TAC-HashV41.hash"
        ./runPerf.sh
        $env:DATA_FILE_NAME=$Null

        # Write out the results for comparison
        Write-Output "Writing performance test results"
        $Results = Get-Content ./summary.json | ConvertFrom-Json
        Write-Output "{
            'HigherIsBetter': {
                'DetectionsPerSecond': $(1/($Results.overhead_ms / 1000))
            },
            'LowerIsBetter': {
                'MsPerDetection': $($Results.overhead_ms)
            }
        }" > $PerfResultsFile

    }
    finally {

        Write-Output "Leaving build"
        Pop-Location

    }
}
finally {

    Write-Output "Leaving '$PerfPath'"
    Pop-Location

}


Write-Output "Entering '$RepoPath'"

Push-Location $RepoPath

try {

    # Run the stress test script. ApacheBench will already be built from the previous test
    Write-Output "Run stress tests using ApacheBench"
    if (Test-Path -Path "tests/performance/build/ApacheBench-prefix/src/ApacheBench-build/bin/ab") {
        $env:DATA_FILE_NAME="TAC-HashV41.hash"
        ./test.sh
        $env:DATA_FILE_NAME=$Null
    }
    else {
        Write-Error 'The performance test need to be build first.'
        exit 1
    }

}
finally {

    Write-Output "Leaving '$RepoPath'"
    Pop-Location

}
