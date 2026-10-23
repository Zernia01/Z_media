$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$ffmpeg = Get-ChildItem (Join-Path $root 'third_party\ffmpeg\*\bin\ffmpeg.exe') | Select-Object -First 1
if (!$ffmpeg) { throw 'Fetch the FFmpeg SDK first.' }
$media = Join-Path $root 'test-media'
New-Item -ItemType Directory -Force $media | Out-Null
& $ffmpeg.FullName -hide_banner -loglevel error -f lavfi -i 'testsrc2=size=1920x1080:rate=60:duration=8' -f lavfi -i 'sine=frequency=440:duration=8' -c:v libopenh264 -b:v 8M -g 120 -c:a aac -y (Join-Path $media 'smoothness.mp4')
if ($LASTEXITCODE -ne 0) { throw "Fixture generation failed: $LASTEXITCODE" }
# A 240 fps timestamp stream stresses the shared video/audio worker without
# introducing an expensive encoder dependency. Audio has its own normal clock.
& $ffmpeg.FullName -hide_banner -loglevel error -itsscale 0.25 -stream_loop 3 -i (Join-Path $media 'smoothness.mp4') -f lavfi -i 'sine=frequency=440:duration=8' -map '0:v:0' -map '1:a:0' -c:v copy -c:a aac -t 8 -y (Join-Path $media 'smoothness-240.mkv')
if ($LASTEXITCODE -ne 0) { throw "High frame rate fixture failed: $LASTEXITCODE" }
if(Test-Path -LiteralPath (Join-Path $media '4k-hevc.mkv')) {
    & $ffmpeg.FullName -hide_banner -loglevel error -stream_loop 3 -i (Join-Path $media '4k-hevc.mkv') -t 8 -c copy -y (Join-Path $media 'smoothness-4k.mkv')
    if ($LASTEXITCODE -ne 0) { throw "4K fixture failed: $LASTEXITCODE" }
}
