param(
    [string]$Name = "1.21.3_dynamic",
    [string]$NginxVersion = "1.21.3",
    [string]$BuildMethod = "dynamic"
)
$ErrorActionPreference = "Stop"
$PSNativeCommandUseErrorActionPreference = $true

Push-Location $PSScriptRoot/..
try {
    # Create the output directory
    $results = New-Item -ItemType directory -Force -Path test-results/integration

    # Disable leak checks here, as they are for the unit tests.
    $env:ASAN_OPTIONS="detect_odr_violation=0:detect_leaks=0"

    Write-Host "Setting up the CDN example"
    # Copy the example config
    Copy-Item examples/hash/jsExample/javascript.conf build/nginx.conf
    # Set the data file
    sed -i "s/51Degrees-LiteV4\.1\.hash/TAC-HashV41\.hash/g" build/nginx.conf
    # Remove the documentation block
    sed -i "/\/\*\*/,/\*\//d" build/nginx.conf
    # Start NGINX
    ./nginx

    # Run the selenium tests
    Push-Location "$PSScriptRoot/../tests/examples/jsExample"
    try {
        Write-Host "Running CDN tests"
        $env:PATH="$($env:PATH):$pwd/driver"
        npm install
        npx browserslist@latest --update-db
        & {
            $PSNativeCommandUseErrorActionPreference = $false
            npm test
        }
        if ($LASTEXITCODE -ne 0) {
            Write-Host '::warning::CDN tests exited with 1, ignoring due to frequent false negatives'
        }
    }
    finally {
        Pop-Location
        Write-Host "Stopping NGINX"
        ./nginx -s quit
    }

    # Test the examples
    Write-Host "Testing examples"
    make test-examples DONT_CLEAN_TESTS=1 FIFTYONEDEGREES_DATAFILE=TAC-HashV41.hash FIFTYONEDEGREES_FORMATTER='--formatter TAP::Formatter::JUnit' FIFTYONEDEGREES_TEST_OUTPUT=$results/${Name}_Examples.xml
} finally {
    Pop-Location
}
