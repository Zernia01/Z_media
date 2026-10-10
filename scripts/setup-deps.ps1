$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$packages = @(
    @{ Name='ffmpeg'; Url='https://github.com/BtbN/FFmpeg-Builds/releases/download/autobuild-2026-10-05-13-07/ffmpeg-n9.0.2-22-g46d8f462ee-win64-lgpl-shared-9.0.zip'; Hash='A299F84209D0B374FDC17BC8A234BBF4942F279F4ADA67DC21A62DE69B6E9933' },
    @{ Name='sdl'; Url='https://www.libsdl.org/release/SDL2-devel-2.32.10-VC.zip'; Hash='AF347939395A58B365846AAEA27391E69F9EC9D4DD650D6AC40802159B418A6E' },
    @{ Name='ttf'; Url='https://github.com/libsdl-org/SDL_ttf/releases/download/release-2.24.0/SDL2_ttf-devel-2.24.0-VC.zip'; Hash='2DEA8EA01E04756EAD27E196A681034ED71342562A75B512EA279FCDFDE83307' }
)
$deps = Join-Path $root 'third_party'
New-Item -ItemType Directory -Force $deps | Out-Null
foreach ($package in $packages) {
    $archive = Join-Path $deps ($package.Name + '-sdk.zip')
    if (!(Test-Path -LiteralPath $archive)) {
        Invoke-WebRequest -Uri $package.Url -OutFile $archive
    }
    if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne $package.Hash) {
        throw "SHA256 mismatch for $($package.Name). Archive was not extracted."
    }
    Expand-Archive -LiteralPath $archive -DestinationPath (Join-Path $deps $package.Name) -Force
    Write-Host "$($package.Name): verified and extracted"
}
