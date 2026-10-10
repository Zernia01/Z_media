$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Drawing
$root=Split-Path $PSScriptRoot -Parent
$source=[System.Drawing.Image]::FromFile((Join-Path $root 'icon.png'))
$sizes=@(16,24,32,48,64,128,256)
$images=@()
try {
    foreach($size in $sizes) {
        $bitmap=[System.Drawing.Bitmap]::new($size,$size,[System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        $graphics=[System.Drawing.Graphics]::FromImage($bitmap)
        $graphics.InterpolationMode=[System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $graphics.PixelOffsetMode=[System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $graphics.DrawImage($source,0,0,$size,$size)
        $stream=[System.IO.MemoryStream]::new()
        $bitmap.Save($stream,[System.Drawing.Imaging.ImageFormat]::Png)
        $images+=,@($stream.ToArray())
        $stream.Dispose();$graphics.Dispose();$bitmap.Dispose()
    }
    $file=[System.IO.File]::Create((Join-Path $root 'resources\zm.ico'))
    $writer=[System.IO.BinaryWriter]::new($file)
    try {
        $writer.Write([uint16]0);$writer.Write([uint16]1);$writer.Write([uint16]$sizes.Count)
        $offset=6+16*$sizes.Count
        for($i=0;$i -lt $sizes.Count;$i++) {
            $dimension=if($sizes[$i] -eq 256){0}else{$sizes[$i]}
            $writer.Write([byte]$dimension);$writer.Write([byte]$dimension)
            $writer.Write([byte]0);$writer.Write([byte]0)
            $writer.Write([uint16]1);$writer.Write([uint16]32)
            $writer.Write([uint32]$images[$i].Length);$writer.Write([uint32]$offset)
            $offset+=$images[$i].Length
        }
        foreach($bytes in $images){$writer.Write([byte[]]$bytes)}
    } finally {$writer.Dispose();$file.Dispose()}
} finally {$source.Dispose()}
Write-Host 'Created resources/zm.ico (16–256 px) from icon.png'
