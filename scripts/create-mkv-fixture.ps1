param([string]$FFmpeg = 'ffmpeg')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$output=Join-Path $root 'test-media\mkv-flac-burst.mkv'
New-Item -ItemType Directory -Force (Split-Path $output) | Out-Null
# Test-only encoder: use a local FFmpeg build with libx265. No encoder or
# copyrighted input media is added to the player distribution.
& $FFmpeg -hide_banner -loglevel error -f lavfi -i 'testsrc2=size=640x360:rate=24000/1001:duration=16' -f lavfi -i 'sine=frequency=440:sample_rate=48000:duration=16' -map '0:v:0' -map '1:a:0' -vf 'format=yuv420p10le' -c:v libx265 -preset ultrafast -x265-params 'pools=2:frame-threads=2:bframes=8:rc-lookahead=16:keyint=48:log-level=error' -c:a flac -sample_fmt s32 -audio_preload 700000 -t 16 -y $output
if($LASTEXITCODE -ne 0) { throw 'MKV fixture generation failed; inspect FFmpeg output (libx265 is required).' }
Write-Output $output
