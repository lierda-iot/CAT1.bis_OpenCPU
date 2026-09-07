param([string]$RepoRoot = (Get-Location).Path)

Add-Type -AssemblyName System.Drawing

function Convert-MenuImage([string]$Source, [string]$Target, [string]$Symbol) {
    $src = [Drawing.Bitmap]::FromFile($Source)
    $bmp = [Drawing.Bitmap]::new(360, 360, [Drawing.Imaging.PixelFormat]::Format24bppRgb)
    try {
        $graphics = [Drawing.Graphics]::FromImage($bmp)
        try {
            $graphics.Clear([Drawing.Color]::White)
            $graphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
            $graphics.PixelOffsetMode = [Drawing.Drawing2D.PixelOffsetMode]::HighQuality
            $graphics.DrawImage($src, [Drawing.Rectangle]::new(0, 0, 360, 360))
        } finally { $graphics.Dispose() }

        $histogram = @{}
        for ($y = 0; $y -lt 360; $y += 2) {
            for ($x = 0; $x -lt 360; $x += 2) {
                $c = $bmp.GetPixel($x, $y)
                $r = ([int]$c.R) -shr 4
                $g = ([int]$c.G) -shr 4
                $b = ([int]$c.B) -shr 4
                $key = ($r -shl 8) -bor ($g -shl 4) -bor $b
                $histogram[$key] = 1 + ($histogram[$key] -as [int])
            }
        }

        $keys = @($histogram.GetEnumerator() | Sort-Object Value -Descending |
                  Select-Object -First 16 | ForEach-Object { [int]$_.Key })
        while ($keys.Count -lt 16) { $keys += 0 }
        $palette = @()
        foreach ($key in $keys) {
            $palette += ,([Drawing.Color]::FromArgb(
                (($key -shr 8) -band 15) * 17,
                (($key -shr 4) -band 15) * 17,
                ($key -band 15) * 17))
        }

        $builder = [Text.StringBuilder]::new()
        $attribute = 'LV_ATTRIBUTE_IMG_' + $Symbol.ToUpper()
        @(
            '#if defined(LV_LVGL_H_INCLUDE_SIMPLE)', '#include "lvgl.h"', '#else',
            '#include "lvgl/lvgl.h"', '#endif', '', '#ifndef LV_ATTRIBUTE_MEM_ALIGN',
            '#define LV_ATTRIBUTE_MEM_ALIGN', '#endif', '', "#ifndef $attribute",
            "#define $attribute", '#endif', '',
            "const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST $attribute uint8_t ${Symbol}_map[] = {"
        ) | ForEach-Object { [void]$builder.AppendLine($_) }
        foreach ($c in $palette) {
            [void]$builder.AppendFormat("  0x{0:x2}, 0x{1:x2}, 0x{2:x2}, 0xff,`r`n", $c.B, $c.G, $c.R)
        }

        $column = 0
        for ($y = 0; $y -lt 360; $y++) {
            for ($x = 0; $x -lt 360; $x += 2) {
                $packed = 0
                for ($n = 0; $n -lt 2; $n++) {
                    $c = $bmp.GetPixel($x + $n, $y)
                    $best = 0
                    $bestDistance = [int]::MaxValue
                    for ($i = 0; $i -lt 16; $i++) {
                        $dr = ([int]$c.R) - ([int]$palette[$i].R)
                        $dg = ([int]$c.G) - ([int]$palette[$i].G)
                        $db = ([int]$c.B) - ([int]$palette[$i].B)
                        $distance = $dr * $dr + $dg * $dg + $db * $db
                        if ($distance -lt $bestDistance) { $bestDistance = $distance; $best = $i }
                    }
                    if ($n -eq 0) { $packed = $best -shl 4 } else { $packed = $packed -bor $best }
                }
                if (($column % 16) -eq 0) { [void]$builder.Append('  ') }
                [void]$builder.AppendFormat('0x{0:x2},', $packed)
                $column++
                if (($column % 16) -eq 0) { [void]$builder.AppendLine() } else { [void]$builder.Append(' ') }
            }
        }
        @('};', '', "const lv_img_dsc_t $Symbol = {", '  .header.always_zero = 0,',
          '  .header.w = 360,', '  .header.h = 360,', '  .data_size = 64864,',
          '  .header.cf = LV_IMG_CF_INDEXED_4BIT,', "  .data = ${Symbol}_map,", '};') |
            ForEach-Object { [void]$builder.AppendLine($_) }
        [IO.File]::WriteAllText($Target, $builder.ToString(), [Text.Encoding]::ASCII)

        $preview = [Drawing.Bitmap]::new(360, 360)
        try {
            for ($y = 0; $y -lt 360; $y++) {
                for ($x = 0; $x -lt 360; $x++) {
                    $c = $bmp.GetPixel($x, $y); $best = 0; $bestDistance = [int]::MaxValue
                    for ($i = 0; $i -lt 16; $i++) {
                        $dr = ([int]$c.R) - ([int]$palette[$i].R)
                        $dg = ([int]$c.G) - ([int]$palette[$i].G)
                        $db = ([int]$c.B) - ([int]$palette[$i].B)
                        $distance = $dr * $dr + $dg * $dg + $db * $db
                        if ($distance -lt $bestDistance) { $bestDistance = $distance; $best = $i }
                    }
                    $preview.SetPixel($x, $y, $palette[$best])
                }
            }
            $preview.Save($Target + '.preview.png', [Drawing.Imaging.ImageFormat]::Png)
        } finally { $preview.Dispose() }
    } finally { $bmp.Dispose(); $src.Dispose() }
}

$output = Join-Path $RepoRoot 'examples/L_CT4IT02_1698W/driver/src/lvgl/generated/images_opt'
$desktopImages = Get-ChildItem "$env:USERPROFILE/Desktop" -File -Filter '*.jpg'
$salarySource = ($desktopImages | Where-Object Length -eq 59516 | Select-Object -First 1).FullName
$positionSource = ($desktopImages | Where-Object Length -eq 67010 | Select-Object -First 1).FullName
Convert-MenuImage "$env:USERPROFILE/Desktop/baji.jpg" (Join-Path $output '_baji_360x360.c') '_baji_360x360'
Convert-MenuImage $salarySource (Join-Path $output '_salary_360x360.c') '_salary_360x360'
Convert-MenuImage $positionSource (Join-Path $output '_positioning_360x360.c') '_positioning_360x360'
Convert-MenuImage "$env:USERPROFILE/Desktop/zitai.jpg" (Join-Path $output '_attitude_360x360.c') '_attitude_360x360'
