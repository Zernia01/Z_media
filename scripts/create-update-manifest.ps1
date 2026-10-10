$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$version = & (Join-Path $PSScriptRoot 'get-version.ps1')
$installer = Join-Path $root "dist\ZM-$version-setup-win64.exe"
$archive = Join-Path $root "dist\ZM-$version-win64.zip"
if (!(Test-Path -LiteralPath $installer) -or !(Test-Path -LiteralPath $archive)) { throw 'Build the installer and portable ZIP first.' }
$manifest = [ordered]@{
    version = $version
    url = "https://github.com/Zernia01/Z_media/releases/download/v$version/ZM-$version-setup-win64.exe"
    sha256 = (Get-FileHash -LiteralPath $installer -Algorithm SHA256).Hash.ToLowerInvariant()
}
$manifest | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $root 'dist\update.json') -Encoding utf8NoBOM
$checksums = foreach ($file in @($installer, $archive, (Join-Path $root 'dist\update.json'))) {
    '{0}  {1}' -f (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash.ToLowerInvariant(), (Split-Path $file -Leaf)
}
$checksums | Set-Content -LiteralPath (Join-Path $root 'dist\SHA256SUMS.txt') -Encoding ascii
Write-Host "Update manifest: $version"
