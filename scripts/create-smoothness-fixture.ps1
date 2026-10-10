$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$ffmpeg = Get-ChildItem (Join-Path $root 'third_party\ffmpeg\*\bin\ffmpeg.exe') | Select-Object -First 1
if (!$ffmpeg) { throw 'Fetch the FFmpeg SDK first.' }
$media = Join-Path $root 'test-media'
New-Item -ItemType Directory -Force $media | Out-Null
& $ffmpeg.FullName -hide_banner -loglevel error -f lavfi -i 'testsrc2=size=1920x1080:rate=60:duration=8' -f lavfi -i 'sine=frequency=440:duration=8' -c:v libopenh264 -b:v 8M -g 120 -c:a aac -y (Join-Path $media 'smoothness.mp4')
if ($LASTEXITCODE -ne 0) { throw "Fixture generation failed: $LASTEXITCODE" }
