param(
    [string]$Name = "1.21.3_dynamic",
    [string]$NginxVersion = "1.21.3",
    [string]$BuildMethod = "dynamic"
)
$ErrorActionPreference = "Stop"
$PSNativeCommandUseErrorActionPreference = $true

$repo = (Get-Item $PSScriptRoot/..).FullName
$summary = New-Item -ItemType directory -Force -Path $repo/test-results/performance-summary

# Write the results of the last runPerf.sh run for comparison under the
# given configuration name.
function Write-Results {
    param([Parameter(Mandatory)][string]$ResultsName)
    $results = Get-Content -Raw ./summary.json | ConvertFrom-Json
    @{
        HigherIsBetter = @{
            DetectionsPerSecond = 1/($results.overhead_ms / 1000)
        }
        LowerIsBetter = @{
            AvgMillisecsPerDetection = $results.overhead_ms
        }
    } | ConvertTo-Json > $summary/results_$ResultsName.json
}

# The IP intelligence results are published under the _IPI suffixed
# configuration name so that they get their own performance graph. Each
# configuration runs only its own engine's performance test and writes
# the results under its own name.
$ipiOnly = $Name -like '*_IPI'

$build = New-Item -ItemType directory -Force -Path "$repo/tests/performance/build"
Push-Location $build
try {
    # Build the performance tests
    Write-Host "Building performance test"
    cmake ..
    cmake --build .

    if (-not $ipiOnly) {
        # When running the performance tests, set the data file name manually,
        # then unset once we're done
        Write-Host "Running device detection performance test"
        $env:DATA_FILE_NAME="TAC-HashV41.hash"
        ./runPerf.sh
        $env:DATA_FILE_NAME=$Null

        Write-Host "Writing device detection performance test results"
        Write-Results $Name
    }

    if ($ipiOnly) {
        # Run the performance test for the IP intelligence module using the
        # Asn data file included in the ip-intelligence-data sub-module.
        Write-Host "Running IP intelligence performance test"
        $env:ENGINE="ipi"
        ./runPerf.sh
        $env:ENGINE=$Null

        Write-Host "Writing IP intelligence performance test results"
        Write-Results $Name
    }
} finally {
    Pop-Location
}


if (-not $ipiOnly) {
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
}
