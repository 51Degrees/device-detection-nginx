param(
    [Parameter(Mandatory=$true)]
    [string]$RepoName,
    [string]$Name = "1.21.3 dynamic",
    [string]$NginxVersion = "1.21.3",
    [string]$NginxPlusVersion = "25",
    [bool]$FullTests = $False,
    [string]$BuildMethod = "dynamic"
)

$RepoPath = [IO.Path]::Combine($pwd, $RepoName)
$TestsPath = [IO.Path]::Combine($RepoPath, "tests")
$JsExamplePath = [IO.Path]::Combine($RepoPath, "tests", "examples", "jsExample")

# Install perl libraries
Write-Output 'Install perl libraries for test output formatters'
# cpan will ask for auto configuration first time it is run so answer 'yes'.
Write-Output y | sudo cpan
sudo cpan  App::cpanminus --notest
sudo cpanm --force TAP::Formatter::JUnit --notest

# Get the version of nginx-test to use. Where new tests have been introduced, previous versions
# of NGINX do not always pass them
$ParsedVersion = [System.Version]::Parse($NginxVersion)
if ($ParsedVersion -le [System.Version]::Parse("1.19.5")) {
    $TestCommit = "6bf30e564c06b404876f0bd44ace8431b3541f24"
}
elseif ($ParsedVersion -le [System.Version]::Parse("1.23.2")) {
    $TestCommit = "3356f91a3fdae372d0946e4c89a9413f558c8017"
}
else {
    $TestCommit = $Null
}


# Download the nginx-tests repository
Write-Output "Moving into $TestsPath"
Push-Location $TestsPath
try {
    Write-Output "Clean any existing nginx-tests folder."
    if (Test-Path -Path nginx-tests) {
        Remove-Item nginx-tests -Recurse -Force
    }

    Write-Output "Clone the nginx-tests source."
    git clone https://github.com/nginx/nginx-tests.git

    Write-Output "Moving into nginx-tests."
    Push-Location nginx-tests

    try {
        if ($Null -ne $TestCommit) {
            Write-Output "Checkout to before the breaking changes for this version."
            git reset --hard $TestCommit
        }
    }
    finally {
        Pop-Location
    }
}
finally {
    Pop-Location
}

# If full tests are requested, then we'll need to install NGINX Plus
if ($FullTests -eq $True) {
    Write-Output "Uninstall existing Nginx"
    sudo apt-get purge nginx -y

    Write-Output "Create ssl directory for Nginx Plus"
    sudo mkdir -p /etc/ssl/nginx
    sudo mkdir -p /etc/nginx
    
    Write-Output "Copy the nginx-repo.* file to the created directory"
    sudo cp $([IO.Path]::Combine($RepoPath, "nginx-repo.key")) /etc/ssl/nginx
    sudo cp $([IO.Path]::Combine($RepoPath, "nginx-repo.crt")) /etc/ssl/nginx
    sudo cp $([IO.Path]::Combine($RepoPath, "license.jwt")) /etc/nginx
    
    Write-Output "Setting permissions for the certificates"
    sudo chown -R root:root /etc/ssl/nginx
    sudo chmod -R a+r /etc/ssl/nginx
    sudo chmod -R a+r /etc/nginx
    
    ls -l /etc/ssl
    ls -l /etc/ssl/nginx
    ls -l /etc/nginx
    
    Write-Output "Certificate expiration date:"
    openssl x509 -enddate -noout -in /etc/ssl/nginx/nginx-repo.crt

    Write-Output "Download and add NGINX signing key and App-protect security updates signing key:"
    curl -O https://nginx.org/keys/nginx_signing.key && sudo apt-key add ./nginx_signing.key

    Write-Output "Install apt utils"
    sudo apt-get install apt-transport-https lsb-release ca-certificates wget gnupg2 ubuntu-keyring

    Write-Output "Add Nginx Plus repository"
    # Add allow-insecure because there is an issue with the signing of the NGINX Plus repository.
    printf "deb [signed-by=/usr/share/keyrings/nginx-archive-keyring.gpg allow-insecure=yes] https://pkgs.nginx.com/plus/ubuntu $(lsb_release -cs) nginx-plus\n" | sudo tee /etc/apt/sources.list.d/nginx-plus.list
    
    Write-Output "Download nginx-plus apt configuration files to /etc/apt/apt.conf.d"
    sudo wget -P /etc/apt/apt.conf.d https://cs.nginx.com/static/files/90pkgs-nginx
    
    Write-Output "Update the repository and install Nginx Plus"
    sudo apt-get update
    $packageVersion = ((apt-cache show nginx-plus | Select-String -CaseSensitive '^Version: ') -replace 'Version: ' -clike "$NginxPlusVersion-*")[0]
    sudo apt-get install nginx-plus=$packageVersion -y --allow-unauthenticated

    Write-Output "Check if installation was successful"
    /usr/sbin/nginx -v 2>&1 | grep -F $NginxVersion || $(throw "Failed to install Nginx Plus $NginxVersion")
}

Write-Output "Install Apache dev for performance tests"
sudo apt-get update -y
sudo apt-get install cmake apache2-dev libapr1-dev libaprutil1-dev -y

# Install any extra requirements for the module
Write-Output "Installing NGINX dependencies"
sudo apt-get install make zlib1g-dev libpcre3 libpcre3-dev

