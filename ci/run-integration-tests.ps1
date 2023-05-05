
param(
    [Parameter(Mandatory=$true)]
    [string]$RepoName,
    [string]$Name = "1.21.3_dynamic",
    [string]$NginxVersion = "1.21.3",
    [string]$BuildMethod = "dynamic"
)

$RepoPath = [IO.Path]::Combine($pwd, $RepoName)

Write-Output "Entering '$RepoPath'"
Push-Location $RepoPath

try {

    # Create the output directories if they don't already exist.
    if ($(Test-Path -Path "test-results") -eq  $False) {
        mkdir test-results
    }
    if ($(Test-Path -Path "test-results/integration") -eq  $False) {
        mkdir test-results/integration
    }
    
    # Disable leak checks here, as they are for the unit tests.
    $env:ASAN_OPTIONS="detect_odr_violation=0:detect_leaks=0"

    Write-Output "Setting up the CDN example"
    # Copy the example config
    Copy-Item examples/hash/jsExample/javascript.conf build/nginx.conf
    # Set the data file
    sed -i "s/51Degrees-LiteV4\.1\.hash/TAC-HashV41\.hash/g" build/nginx.conf
    # Remove the documentation block
    sed -i "/\/\*\*/,/\*\//d" build/nginx.conf
    # Start NGINX
    ./nginx

    # Run the selenium tests
    $JsExamplePath = [IO.Path]::Combine($RepoPath, "tests", "examples", "jsExample")
    Write-Output "Entering '$JsExamplePath'"
    Push-Location $JsExamplePath

    try {
        Write-Output "Running CDN tests"
        $env:PATH="$($env:PATH):$pwd/driver"
        npm install
        npx browserslist@latest --update-db
        npm test
    }
    finally {
        
        Write-Output "Leaving '$JsExamplePath'"
        Pop-Location
        Write-Output "Stopping NGINX"
        ./nginx -s quit
        if ($LASTEXITCODE -ne 0 ) {
            Write-Error "Failed to exit Nginx"
            exit 1
        }

    }

    # Test the examples
    Write-Output "Testing examples"
    make test-examples FIFTYONEDEGREES_DATAFILE=TAC-HashV41.hash FIFTYONEDEGREES_FORMATTER='--formatter TAP::Formatter::JUnit' FIFTYONEDEGREES_TEST_OUTPUT=$RepoPath/test-results/integration/$($Name)_Examples.xml

}
finally {

    Write-Output "Leaving '$RepoPath'"
    Pop-Location

}
