param(
    [Parameter(Mandatory)][string]$RepoName,
    [string[]]$Images = "ubuntu-latest"
)
$ErrorActionPreference = "Stop"

Write-Host "Fetching releases document..."
$req = Invoke-WebRequest "https://raw.githubusercontent.com/nginx/documentation/refs/heads/main/content/nginx/releases.md"

Write-Host "Parsing the table of supported releases..."
$tableStart = $req.Content.IndexOf("| NGINX Plus Release |")
$tableEnd = $req.Content.IndexOf("`n`n", $tableStart)
$supportedReleases = ([regex]'(?m)^\| \[R(\d+)\]').Matches($req.Content.Substring($tableStart, $tableEnd-$tableStart)) | ForEach-Object { $_.Groups[1].Value }
Write-Host "Supported NGINX Plus releases: $supportedReleases"

if (!$supportedReleases) {
    Write-Error "Failed to parse the list of supported releases"
}

Write-Host "Searching for corresponding Open Source versions of each release..."
$releaseRegex = [regex]'(?mx)                          # enable multiline mode and allow comments/spaces
    ^\#\#\sNGINX\sPlus\sRelease\s(\d+)\s\(R\d+\) .* \n # \s is required to match non-breaking spaces that the document uses
    .* \n                                              # discard release date since we know supported versions from the table
    _?Based\son\sNGINX\sOpen\sSource\s([0-9.]+)
'
$openSourceOf = @{}
$releaseRegex.Matches($req.Content) | ForEach-Object { $openSourceOf[$_.Groups[1].Value] = $_.Groups[2].Value }
if (!$openSourceOf.Count) {
    Write-Error "Failed to parse the list of releases"
}

Write-Host "Building options array..."
[Collections.ArrayList]$options = @()
foreach ($release in $supportedReleases) {
    $isLatest = $options.Count -lt 1
    foreach ($image in $Images) {
        [void]$options.Add([ordered]@{
            Image = $image
            Name = "$($image)_Nginx$($openSourceOf[$release])"
            NginxVersion = $openSourceOf[$release]
            NginxPlusVersion = $release
            RunPerformance = $IsLatest # Only run performance if this is the latest NGINX Plus version
            FullTests = $IsLatest # Only run the full NGINX test suite if this is the latest NGINX Plus version
            PackageRequirement = $True
        })
        if ($isLatest) {
            # Add memory check configurations if this is the latest NGINX Plus version
            [void]$options.Add([ordered]@{
                Image = $image
                Name = "$($image)_Nginx$($openSourceOf[$release])_MemCheck"
                NginxVersion = $openSourceOf[$release]
                MemCheck = $True
            })
            [void]$options.Add([ordered]@{
                Image = $image
                Name = "$($image)_Nginx$($openSourceOf[$release])_MemCheck_Static"
                NginxVersion = $openSourceOf[$release]
                BuildMethod = "static"
                MemCheck = $True
            })
        }
    }
}

Write-Host "Writing options file to '$RepoName/ci/options.json'"
$Options | ConvertTo-Json > "$RepoName/ci/options.json"
Get-Content "$RepoName/ci/options.json"
