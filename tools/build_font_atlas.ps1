param(
    [Parameter(Mandatory = $true)] [string]$MsdfAtlasGen,
    [Parameter(Mandatory = $true)] [string]$Font,
    [string]$OutputDirectory = 'assets\fonts'
)

$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$temporaryDirectory = Join-Path ([System.IO.Path]::GetTempPath()) ('psplib-font-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force -Path $temporaryDirectory | Out-Null

try {
    $charset = (0x20..0x7E + 0x400..0x45F | ForEach-Object { '0x{0:X}' -f $_ }) -join ', '
    $charsetPath = Join-Path $temporaryDirectory 'charset.txt'
    $imagePath = Join-Path $temporaryDirectory 'atlas.t8'
    $jsonPath = Join-Path $temporaryDirectory 'atlas.json'
    Set-Content -LiteralPath $charsetPath -Value $charset -Encoding ASCII

    & $MsdfAtlasGen -font $Font -charset $charsetPath -type sdf -format bin `
        -imageout $imagePath -json $jsonPath -size 24 -pxrange 4 -pots -yorigin top -nokerning
    if ($LASTEXITCODE -ne 0) { throw "msdf-atlas-gen failed with exit code $LASTEXITCODE" }

    $data = Get-Content -LiteralPath $jsonPath -Raw | ConvertFrom-Json
    $atlas = $data.atlas
    $metrics = $data.metrics
    $glyphs = @($data.glyphs)
    $lookup = New-Object 'System.UInt16[]' 65536
    for ($i = 0; $i -lt $glyphs.Count; ++$i) {
        $codepoint = [int]$glyphs[$i].unicode
        if ($codepoint -ge 0 -and $codepoint -lt 65536) { $lookup[$codepoint] = [uint16]($i + 1) }
    }

    $metaPath = Join-Path $OutputDirectory 'pspl_noto_sans_sdf.meta'
    $atlasOutputPath = Join-Path $OutputDirectory 'pspl_noto_sans_sdf.t8'
    $stream = [System.IO.File]::Open($metaPath, [System.IO.FileMode]::Create)
    $writer = New-Object System.IO.BinaryWriter($stream)
    try {
        $writer.Write([System.Text.Encoding]::ASCII.GetBytes('T8S1'))
        $writer.Write([uint32]$atlas.width)
        $writer.Write([uint32]$atlas.height)
        $writer.Write([single]$metrics.lineHeight)
        $writer.Write([single]$metrics.ascender)
        $writer.Write([single]$atlas.size)
        $writer.Write([uint32]$glyphs.Count)
        foreach ($value in $lookup) { $writer.Write([uint16]$value) }
        foreach ($glyph in $glyphs) {
            $writer.Write([uint32]$glyph.unicode)
            if ($null -eq $glyph.atlasBounds -or $null -eq $glyph.planeBounds) {
                0..6 | ForEach-Object { $writer.Write([single]0) }
                continue
            }
            $writer.Write([single]($glyph.atlasBounds.left / $atlas.width))
            $writer.Write([single]($glyph.atlasBounds.top / $atlas.height))
            $writer.Write([single](($glyph.atlasBounds.right - $glyph.atlasBounds.left) / $atlas.width))
            $writer.Write([single](($glyph.atlasBounds.bottom - $glyph.atlasBounds.top) / $atlas.height))
            $writer.Write([single]$glyph.planeBounds.left)
            $writer.Write([single]$glyph.planeBounds.top)
            $writer.Write([single]$glyph.advance)
        }
    } finally {
        $writer.Dispose()
        $stream.Dispose()
    }
    Copy-Item -LiteralPath $imagePath -Destination $atlasOutputPath -Force
    Write-Output ("Generated {0} glyphs, {1}x{2} T8 atlas: {3}" -f $glyphs.Count, $atlas.width, $atlas.height, $OutputDirectory)
} finally {
    if (Test-Path -LiteralPath $temporaryDirectory) {
        Remove-Item -LiteralPath $temporaryDirectory -Recurse -Force
    }
}
