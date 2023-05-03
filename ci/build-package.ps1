
param(
    [Parameter(Mandatory=$true)]
    [string]$RepoName
)

$RepoPath = [IO.Path]::Combine($pwd, $RepoName)

       
# Create a directory for binary files from which they will be uploaded
# as artifacts.
New-Item -path "$RepoPath/package" -ItemType Directory -Force 

# Copy all modules
Copy-Item -r "$RepoPath/package-files/*" "$RepoPath/package/"
