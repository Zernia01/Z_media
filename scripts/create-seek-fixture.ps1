$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$ffmpeg = Get-ChildItem (Join-Path $root 'third_party\ffmpeg\*\bin\ffmpeg.exe') | Select-Object -First 1
if (!$ffmpeg) { throw 'Fetch the FFmpeg SDK first.' }
$media = Join-Path $root 'test-media'
New-Item -ItemType Directory -Force $media | Out-Null
$subtitle = Join-Path $media 'long-seek-input.srt'
@'
1
00:01:00,000 --> 00:01:30,000
EARLY 한글 자막

2
00:09:50,000 --> 00:10:10,000
LATE 日本語 中文
'@ | Set-Content -LiteralPath $subtitle -Encoding utf8
& $ffmpeg.FullName -hide_banner -loglevel error -f lavfi -i 'color=c=black:s=320x180:r=24:d=720' -f lavfi -i 'sine=frequency=440:duration=720' -i $subtitle -map 0:v -map 1:a -map 2:s -c:v libopenh264 -b:v 300k -g 120 -pix_fmt yuv420p -c:a aac -c:s srt -y (Join-Path $media 'long-seek.mkv')
if ($LASTEXITCODE -ne 0) { throw "Fixture generation failed: $LASTEXITCODE" }
