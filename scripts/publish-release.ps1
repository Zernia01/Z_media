param([string]$RepositoryPath = (Split-Path $PSScriptRoot -Parent))
$ErrorActionPreference = 'Stop'
$version = & (Join-Path $PSScriptRoot 'get-version.ps1')
$root = Split-Path $PSScriptRoot -Parent
& (Join-Path $PSScriptRoot 'create-update-manifest.ps1')
$env:GIT_TERMINAL_PROMPT = '0'
$env:GCM_INTERACTIVE = 'never'
$credential = @{}
@('protocol=https','host=github.com','') | git credential fill | ForEach-Object {
    $pair = $_ -split '=',2
    if ($pair.Count -eq 2) { $credential[$pair[0]] = $pair[1] }
}
if (!$credential.password) { throw 'GitHub Git credentials are unavailable. Sign in with Git Credential Manager first.' }
$headers = @{ Authorization = "Bearer $($credential.password)"; Accept = 'application/vnd.github+json'; 'X-GitHub-Api-Version' = '2022-11-28' }
$api = 'https://api.github.com/repos/Zernia01/Z_media'
$releases = Invoke-RestMethod -Uri "$api/releases" -Headers $headers
if ($releases | Where-Object { $_.tag_name -eq "v$version" }) { throw "Release v$version already exists. Choose a new version instead of replacing published installers." }
$commit = & git -C $RepositoryPath rev-parse HEAD
if ($LASTEXITCODE -ne 0) { throw 'Commit the source before publishing.' }
$notes = Get-Content -LiteralPath (Join-Path $root 'docs\release-notes.md') -Raw
$body = @{ tag_name="v$version"; target_commitish=$commit; name="Zernia media (ZM) $version"; body=$notes; draft=$true; prerelease=$false } | ConvertTo-Json
$release = Invoke-RestMethod -Uri "$api/releases" -Method Post -Headers $headers -Body $body -ContentType 'application/json; charset=utf-8'
foreach ($name in @("ZM-$version-setup-win64.exe", "ZM-$version-win64.zip", 'update.json', 'SHA256SUMS.txt')) {
    $uri = "https://uploads.github.com/repos/Zernia01/Z_media/releases/$($release.id)/assets?name=$([uri]::EscapeDataString($name))"
    $asset = Invoke-RestMethod -Uri $uri -Method Post -Headers $headers -InFile (Join-Path $root "dist\$name") -ContentType 'application/octet-stream'
    Write-Host "Uploaded: $($asset.name) ($($asset.size) bytes)"
}
$published = Invoke-RestMethod -Uri "$api/releases/$($release.id)" -Method Patch -Headers $headers -Body '{"draft":false,"make_latest":"true"}' -ContentType 'application/json'
$credential.Clear(); $headers.Clear()
Write-Host $published.html_url
