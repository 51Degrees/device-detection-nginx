
param(
    [Parameter(Mandatory=$true)]
    [string]$RepoName
)


       
# Create a directory for binary files from which they will be uploaded
# as artifacts.
New-Item -path "package" -ItemType Directory -Force 

# Copy all modules
Copy-Item -r "package-files/*" "package/"
