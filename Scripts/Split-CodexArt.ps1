$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$artFolder = Join-Path $PSScriptRoot '..\ContentSource\Meshy\CodexUI'
$atlas = [System.Drawing.Bitmap]::new((Join-Path $artFolder 'CodexAtlas.png'))
$names = @('BoneLance','EmberFan','FrostNova','RaiseSkeletons','School_Necromancy','School_Fire','School_Blood','School_Curses','School_Frost')
try {
    for ($i = 0; $i -lt 9; $i++) {
        $col = $i % 3; $row = [int][Math]::Floor($i / 3)
        $left = [int][Math]::Round($col * $atlas.Width / 3); $top = [int][Math]::Round($row * $atlas.Height / 3)
        $right = [int][Math]::Round(($col + 1) * $atlas.Width / 3); $bottom = [int][Math]::Round(($row + 1) * $atlas.Height / 3)
        $crop = $atlas.Clone([System.Drawing.Rectangle]::new($left,$top,$right-$left,$bottom-$top), $atlas.PixelFormat)
        try { $crop.Save((Join-Path $artFolder ($names[$i]+'.png')), [System.Drawing.Imaging.ImageFormat]::Png) }
        finally { $crop.Dispose() }
    }
} finally { $atlas.Dispose() }
