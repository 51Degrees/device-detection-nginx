
param(
    [Parameter(Mandatory=$true)]
    [string]$RepoName
)

$Images = @("ubuntu-latest")

$RepoPath = [IO.Path]::Combine($pwd, $RepoName)
$OptionsPath = [IO.Path]::Combine($RepoPath, "ci", "options.json")

Write-Output "Entering '$RepoPath'"
Push-Location $RepoPath

try {

    if (-not (Get-Module -ErrorAction Ignore -ListAvailable PowerHTML)) {
        Write-Output "Installing PowerHTML"
        Install-Module PowerHTML -Force
    }
    Import-Module PowerHTML

    # This is the 24 months that NGINX Plus releases are supported for.
    $EosdDays = 365 * 2
    $CurrentDate = Get-Date
    $VersionsUrl = "https://docs.nginx.com/nginx/releases/"

    # This page details the NGINX Plus releases, and which opensource version they
    # are based on. It also contains the date each will be supported until.
    # The format is consistent, so we can parse versions from there.
    Write-Output "Getting versions from '$VersionsUrl'"
    $Html = ConvertFrom-Html -URI $VersionsUrl

    $VersionsToSupport = @()

    Write-Output "Parsing versions"
    foreach ($Heading in $Html.SelectNodes("//h2")) {
        # Replace the &nbsp; characters with spaces when using any of the strings.
        if ($Heading.InnerText.Replace([char]0x00A0, ' ').StartsWith("NGINX Plus Release")) {
            # Get the version from a string like 'NGINX Plus Release 28 (R28)'
            $Version = $Heading.InnerText.Replace([char]0x00A0, ' ').Split(" ")[3]
            # Release date and the opensource version are in the next element.
            $Details = $Heading.NextSibling.SelectNodes("em")
            # Get the date from a string like '29 November 2022'
            $DateString = $Details[0].InnerText
            $ReleaseDate = [DateTime]::Parse($DateString)
            # Get the opensource version from a string like 'Based on NGINX Open Source 1.23.2'
            $OpenSourceVersionString = $Details[1].InnerText
            $OpenSourceVersion = $($OpenSourceVersionString.Replace([char]0x00A0, ' ') -replace "Based on NGINX Open Source ([0-9]+\.[0-9]+\.[0-9]+).*", '$1')
            if ($($CurrentDate - $ReleaseDate) -lt [TimeSpan]::FromDays($EosdDays)) {
                $SupportedVersion = @{}
                $SupportedVersion.Version = $Version
                $SupportedVersion.OpenSourceVersion = $OpenSourceVersion
                $VersionsToSupport += $SupportedVersion
            }
        }
    }

    Write-Output "Building options file"
    $Options = @()
    $IsLatest = $True
    foreach ($SupportedVersion in $VersionsToSupport) {
    
        foreach ($Image in $Images) {
            $Options += @{
                Image = $Image
                Name = "$($Image)_Nginx$($SupportedVersion.OpenSourceVersion)"
                NginxVersion = $SupportedVersion.OpenSourceVersion
                RunPerformance = $IsLatest
                FullTests = $IsLatest
            }
            if ($IsLatest -eq $True) {
                $Options += @{
                    Image = $Image
                    Name = "$($Image)_Nginx$($SupportedVersion.OpenSourceVersion)_MemCheck"
                    NginxVersion = $SupportedVersion.OpenSourceVersion
                    MemCheck = $True
                }
                $Options += @{
                    Image = $Image
                    Name = "$($Image)_Nginx$($SupportedVersion.OpenSourceVersion)_MemCheck_Static"
                    NginxVersion = $SupportedVersion.OpenSourceVersion
                    BuildMethod = "static"
                    MemCheck = $True
                }
            }
        }
        # The versions are ordered by date, so only the first in the array is the latest.
        $IsLatest = $False
    }

    Write-Output "Writing options file to '$OptionsPath'"
    Write-Output "Content is : "
    Write-Output $($Options | ConvertTo-Json)
    $Options | ConvertTo-Json > $OptionsPath
}
finally {

    Write-Output "Leaving '$RepoPath'"
    Pop-Location

}
