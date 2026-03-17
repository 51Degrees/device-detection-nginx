param(
    [string]$Name = "1.21.3_dynamic",
    [string]$NginxVersion = "1.21.3",
    [string]$BuildMethod = "dynamic",
    [bool]$FullTests = $False,
    [bool]$MemCheck = $False
)
$ErrorActionPreference = "Stop"

$failed = $false

$repo = (Get-Item $PSScriptRoot/..).FullName
Push-Location $repo
try {
    # Create the output directory
    $results = New-Item -ItemType directory -Force -Path test-results/unit

    Write-Host "Running 51Degrees unit tests"
    make test FIFTYONEDEGREES_DATAFILE=TAC-HashV41.hash FIFTYONEDEGREES_FORMATTER='--formatter TAP::Formatter::JUnit' FIFTYONEDEGREES_TEST_OUTPUT=$results/$Name.xml || $($failed = $true)
    # Output the full results file, as some bits are not reported in GitHub
    Get-Content -Raw $results/$Name.xml

    if ($FullTests -eq $True) {
        $env:ALLOW_PASSING_TODOS = 'yep' # don't consider Nginx's todo tests as failures

        # If full tests are requested, then run the full test suite (NGINX core tests, with the module enabled)
        Write-Host "Running full NGINX unit tests"
        make test-full FIFTYONEDEGREES_DATAFILE=TAC-HashV41.hash FIFTYONEDEGREES_FORMATTER='--formatter TAP::Formatter::JUnit' FIFTYONEDEGREES_TEST_OUTPUT=$results/${Name}_Full.xml || $($failed = $true)

        # Now do the same with the NGINX Plus executable
        Write-Host "Running full NGINX unit tests against NGINX Plus"

        # For mgmt module we need to set permissions to nginx-mgmt-state
        sudo chmod -R a+xrw /var/lib

        # As we're not using the makefile, we need to set the environment variables ourselves, and add common path to license_token
        $env:TEST_NGINX_BINARY="/usr/sbin/nginx"
        $env:TEST_NGINX_GLOBALS="mgmt {license_token /etc/nginx/license.jwt;} load_module $repo/build/modules/ngx_http_51D_module.so;"
        $env:TEST_NGINX_GLOBALS_HTTP="51D_file_path $repo/device-detection-cxx/device-detection-data/TAC-HashV41.hash;"
        prove --formatter TAP::Formatter::JUnit -v tests/51degrees.t tests/nginx-tests :: TAC-HashV41.hash > $results/${Name}_Full_Plus.xml || $($failed = $true)
        # Unset the environment variables for future tests.
        $env:TEST_NGINX_BINARY=""
        $env:TEST_NGINX_GLOBALS=""
        $env:TEST_NGINX_GLOBALS_HTTP=""
    }
} finally {
    Pop-Location
}

exit $failed ? 1 : 0
