param(
    [string]$Name = "1.21.3_dynamic",
    [string]$NginxVersion = "1.21.3",
    [string]$BuildMethod = "dynamic"
)
$ErrorActionPreference = "Stop"
$PSNativeCommandUseErrorActionPreference = $true

$repo = (Get-Item $PSScriptRoot/..).FullName
$summary = New-Item -ItemType directory -Force -Path $repo/test-results/performance-summary

$build = New-Item -ItemType directory -Force -Path "$repo/tests/performance/build"
Push-Location $build
try {
    # Build the performance tests
    Write-Host "Building performance test"
    cmake ..
    cmake --build .

    # When running the performance tests, set the data file name manually,
    # then unset once we're done
    Write-Host "Running performance test"
    $env:DATA_FILE_NAME="TAC-HashV41.hash"
    ./runPerf.sh
    $env:DATA_FILE_NAME=$Null

    # Write out the results for comparison
    Write-Host "Writing performance test results"
    $results = Get-Content -Raw ./summary.json | ConvertFrom-Json
    @{
        HigherIsBetter = @{
            DetectionsPerSecond = 1/($results.overhead_ms / 1000)
        }
        LowerIsBetter = @{
            AvgMillisecsPerDetection = $results.overhead_ms
        }
    } | ConvertTo-Json > $summary/results_$Name.json
} finally {
    Pop-Location
}


Push-Location $repo
try {
    # Run the stress test script. ApacheBench will already be built from the previous test
    Write-Host "Run stress tests using ApacheBench"
    if (Test-Path -Path "tests/performance/build/ApacheBench-prefix/src/ApacheBench-build/bin/ab") {
        $env:DATA_FILE_NAME="TAC-HashV41.hash"
        ./test.sh
        $env:DATA_FILE_NAME=$Null
    } else {
        Write-Error 'The performance test need to be build first.'
    }
} finally {
    Pop-Location
}
