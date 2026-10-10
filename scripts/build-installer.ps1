$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$version = & (Join-Path $PSScriptRoot 'get-version.ps1')
$compiler = Join-Path $root 'third_party\inno\ISCC.exe'
if (!(Test-Path -LiteralPath $compiler)) {
    $compiler = Join-Path ${env:ProgramFiles(x86)} 'Inno Setup 6\ISCC.exe'
}
if (!(Test-Path -LiteralPath $compiler)) {
    throw 'Install Inno Setup 6 or place its compiler in third_party/inno.'
}
& (Join-Path $PSScriptRoot 'package.ps1')
& $compiler '/Qp' "/DAppVersion=$version" (Join-Path $root 'installer\ZM.iss')
if ($LASTEXITCODE -ne 0) { throw "Installer compilation failed: $LASTEXITCODE" }
Write-Host (Join-Path $root "dist\ZM-$version-setup-win64.exe")
