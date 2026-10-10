param([ValidateSet('Debug','Release')][string]$Configuration = 'Release')
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
& "$PSScriptRoot\build.cmd" $Configuration
if ($LASTEXITCODE) { throw 'Build or tests failed.' }
