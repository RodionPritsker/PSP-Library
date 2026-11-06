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
    # Keep the ordinary and non-breaking spaces in the same generated font set.
    # Both are intentionally allowed to have no atlas geometry while retaining
    # their advance metrics.
    $charset = (0x20..0x7E + 0xA0 + 0x400..0x45F | ForEach-Object { '0x{0:X}' -f $_ }) -join ', '
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
        # T8S2 stores the glyph quad in font-plane coordinates.  T8S1 only
        # stored the atlas rectangle and could therefore stretch glyphs when
        # the SDF padding differed from the real plane bounds.
        $writer.Write([System.Text.Encoding]::ASCII.GetBytes('T8S2'))
        $writer.Write([uint32]$atlas.width)
        $writer.Write([uint32]$atlas.height)
        $writer.Write([single]$metrics.lineHeight)
        $writer.Write([single]$metrics.ascender)
        $writer.Write([single]$atlas.size)
        $writer.Write([uint32]$glyphs.Count)
        foreach ($value in $lookup) { $writer.Write([uint16]$value) }
        foreach ($glyph in $glyphs) {
            $writer.Write([uint32]$glyph.unicode)
            if ($null -ne $glyph.atlasBounds) {
                $writer.Write([single]($glyph.atlasBounds.left / $atlas.width))
                $writer.Write([single]($glyph.atlasBounds.top / $atlas.height))
                $writer.Write([single](($glyph.atlasBounds.right - $glyph.atlasBounds.left) / $atlas.width))
                $writer.Write([single](($glyph.atlasBounds.bottom - $glyph.atlasBounds.top) / $atlas.height))
            } else {
                1..4 | ForEach-Object { $writer.Write([single]0) }
            }
            if ($null -ne $glyph.planeBounds) {
                # -yorigin top makes top the first plane coordinate and bottom
                # the second.  Keep the quad in the same top-down convention
                # used by the PSP GU vertex coordinates.
                $writer.Write([single]$glyph.planeBounds.left)
                $writer.Write([single]$glyph.planeBounds.top)
                $writer.Write([single]($glyph.planeBounds.right - $glyph.planeBounds.left))
                $writer.Write([single]($glyph.planeBounds.bottom - $glyph.planeBounds.top))
            } else {
                1..4 | ForEach-Object { $writer.Write([single]0) }
            }
            # Never discard advance when a glyph has no visible geometry.
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
