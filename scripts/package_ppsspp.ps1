param(
    [string]$EbootPath = "build/EBOOT.PBP",
    [string]$OutputPath = "psp-library-ppsspp.zip"
)

$projectRoot = Split-Path -Parent $PSScriptRoot
$resolvedEboot = Join-Path $projectRoot $EbootPath
$resolvedOutput = Join-Path $projectRoot $OutputPath
$staging = Join-Path $projectRoot ".ppsspp-staging"

if (-not (Test-Path -LiteralPath $resolvedEboot -PathType Leaf)) {
    throw "EBOOT.PBP was not found: $resolvedEboot"
}

try {
    if (Test-Path -LiteralPath $staging) {
        Remove-Item -LiteralPath $staging -Recurse -Force
    }
    $target = Join-Path $staging "PSP\GAME\PSPLIBRARY"
    New-Item -ItemType Directory -Path $target -Force | Out-Null
    Copy-Item -LiteralPath $resolvedEboot -Destination (Join-Path $target "EBOOT.PBP") -Force
    if (Test-Path -LiteralPath $resolvedOutput) {
        Remove-Item -LiteralPath $resolvedOutput -Force
    }
    Compress-Archive -Path (Join-Path $staging "PSP") -DestinationPath $resolvedOutput -CompressionLevel Optimal
    Write-Output "Created $resolvedOutput"
}
finally {
    if (Test-Path -LiteralPath $staging) {
        Remove-Item -LiteralPath $staging -Recurse -Force
    }
}
