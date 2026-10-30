param (
    [Parameter(Mandatory)][string]$RepoName,
    [string]$VariableName = "Version"
)
$ErrorActionPreference = "Stop"

# The next version is one patch above the highest version tag, whether or not
# that tag is on main. Only the patch number moves here.
$tags = @(git -C $RepoName tag --list |
    Where-Object { $_ -match '^\d+\.\d+\.\d+$' } |
    Sort-Object { [version]$_ })
$headTags = @(git -C $RepoName tag --points-at HEAD |
    Where-Object { $_ -match '^\d+\.\d+\.\d+$' })

if ($headTags) {
    # Already released, so the publish sees the tag and does nothing.
    $next = $headTags | Sort-Object { [version]$_ } | Select-Object -Last 1
} else {
    $highest = [version]$tags[-1]
    $next = "$($highest.Major).$($highest.Minor).$($highest.Build + 1)"
}

Set-Variable -Scope Global -Name $VariableName -Value $next
