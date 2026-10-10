$root = Split-Path $PSScriptRoot -Parent
$source = Get-Content -LiteralPath (Join-Path $root 'CMakeLists.txt') -Raw
if ($source -notmatch 'project\(ZerniaMedia VERSION (\d+\.\d+\.\d+)') { throw 'Project version was not found.' }
$Matches[1]
