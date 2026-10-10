$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$version = & (Join-Path $PSScriptRoot 'get-version.ps1')
$build = Join-Path $root 'build\Release'
if (!(Test-Path -LiteralPath "$build\ZM.exe")) { throw 'Build Release first.' }
$package = Join-Path $root "dist\ZM-$version-win64"
New-Item -ItemType Directory -Force $package | Out-Null
Copy-Item -LiteralPath "$build\ZM.exe" -Destination $package
Get-ChildItem -LiteralPath $build -Filter '*.dll' | Copy-Item -Destination $package
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vsRoot = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$crt = Get-ChildItem "$vsRoot\VC\Redist\MSVC\*\x64\Microsoft.VC*.CRT" -Directory | Sort-Object FullName -Descending | Select-Object -First 1
if (!$crt) { throw 'Visual C++ app-local runtime directory was not found.' }
Get-ChildItem -LiteralPath $crt.FullName -Filter '*.dll' | Copy-Item -Destination $package
Copy-Item -LiteralPath "$build\i18n" -Destination $package -Recurse -Force
Copy-Item -LiteralPath "$build\fonts" -Destination $package -Recurse -Force
Copy-Item -LiteralPath "$root\README.md", "$root\THIRD_PARTY.md" -Destination $package
$licenses=Join-Path $package 'licenses'
New-Item -ItemType Directory -Force $licenses | Out-Null
if (Test-Path -LiteralPath "$root\third_party\inno\license.txt") {
    Copy-Item -LiteralPath "$root\third_party\inno\license.txt" -Destination (Join-Path $licenses 'inno-setup.txt')
}
Get-ChildItem "$root\third_party\ffmpeg\*\LICENSE.txt", "$root\third_party\sdl\*\LICENSE.txt", "$root\third_party\ttf\*\LICENSE.txt" -ErrorAction SilentlyContinue | ForEach-Object {
    $name = if ($_.FullName -like '*\ffmpeg\*') { 'ffmpeg.txt' } elseif ($_.FullName -like '*\sdl\*') { 'sdl.txt' } else { 'sdl-ttf.txt' }
    Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $licenses $name)
}
Compress-Archive -Path "$package\*" -DestinationPath "$root\dist\ZM-$version-win64.zip" -Force
Write-Host $package
