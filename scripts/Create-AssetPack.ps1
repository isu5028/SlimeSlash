[CmdletBinding()]
param(
    [string]$OutputPath = ""
)

$ErrorActionPreference = "Stop"

$scriptDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Split-Path -Parent $scriptDirectory

if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    $OutputPath = Join-Path $repoRoot "dist\CubeGame\assets.pack"
}

$assetEntries = @(
    @{ Source = "Project\Assets\Fonts\Shikakufuto_ASCII.ttf"; PackPath = "Project/Assets/Fonts/Shikakufuto_Free_ver20251228.ttf" },
    @{ Source = "Project\Assets\Sounds\BGM.wav"; PackPath = "Project/Assets/Sounds/BGM.wav" },
    @{ Source = "Project\Assets\Sounds\Dash.wav"; PackPath = "Project/Assets/Sounds/Dash.wav" },
    @{ Source = "Project\Assets\Sounds\Jump.wav"; PackPath = "Project/Assets/Sounds/Jump.wav" },
    @{ Source = "Project\Assets\Sounds\PlayerAttackHit.wav"; PackPath = "Project/Assets/Sounds/PlayerAttackHit.wav" },
    @{ Source = "Project\Assets\Sounds\PlayerAttackHit2.wav"; PackPath = "Project/Assets/Sounds/PlayerAttackHit2.wav" },
    @{ Source = "Project\Assets\Sounds\PlayerAttackHit3.wav"; PackPath = "Project/Assets/Sounds/PlayerAttackHit3.wav" },
    @{ Source = "Project\Assets\Sounds\PlayerAttackMiss.wav"; PackPath = "Project/Assets/Sounds/PlayerAttackMiss.wav" },
    @{ Source = "Project\Assets\Sounds\PlayerAttackMiss2.wav"; PackPath = "Project/Assets/Sounds/PlayerAttackMiss2.wav" },
    @{ Source = "Project\Assets\Sounds\PlayerAttackMiss3.wav"; PackPath = "Project/Assets/Sounds/PlayerAttackMiss3.wav" },
    @{ Source = "Project\Assets\Sounds\PlayerDamaged.wav"; PackPath = "Project/Assets/Sounds/PlayerDamaged.wav" },
    @{ Source = "Project\Assets\Sounds\SlimeExplode.wav"; PackPath = "Project/Assets/Sounds/SlimeExplode.wav" },
    @{ Source = "Project\Assets\Sounds\SlimeFloor.wav"; PackPath = "Project/Assets/Sounds/SlimeFloor.wav" },
    @{ Source = "Project\Assets\Sounds\SlimeJump.wav"; PackPath = "Project/Assets/Sounds/SlimeJump.wav" },
    @{ Source = "Project\Assets\Sounds\SlimeRush.wav"; PackPath = "Project/Assets/Sounds/SlimeRush.wav" },
    @{ Source = "Project\Assets\Sounds\SlimeStamp.wav"; PackPath = "Project/Assets/Sounds/SlimeStamp.wav" },
    @{ Source = "Engine\Shaders\VertexShader.hlsl"; PackPath = "Engine/Shaders/VertexShader.hlsl" },
    @{ Source = "Engine\Shaders\PixelShader.hlsl"; PackPath = "Engine/Shaders/PixelShader.hlsl" },
    @{ Source = "Engine\Shaders\SpriteVertexShader.hlsl"; PackPath = "Engine/Shaders/SpriteVertexShader.hlsl" },
    @{ Source = "Engine\Shaders\SpritePixelShader.hlsl"; PackPath = "Engine/Shaders/SpritePixelShader.hlsl" },
    @{ Source = "Engine\Shaders\ColoredPlaneDitherShader.hlsl"; PackPath = "Engine/Shaders/ColoredPlaneDitherShader.hlsl" }
    @{ Source = "Engine\Shaders\AnimatedBackgroundShader.hlsl"; PackPath = "Engine/Shaders/AnimatedBackgroundShader.hlsl" }
)

$imagesRoot = Join-Path $repoRoot "Assets\Images"
Get-ChildItem -LiteralPath $imagesRoot -Recurse -File | ForEach-Object {
    $relativePath = $_.FullName.Substring($repoRoot.Length + 1)
    $assetEntries += @{
        Source = $relativePath
        PackPath = ($relativePath -replace '\\', '/')
    }
}

$records = @()
foreach ($entry in $assetEntries) {
    $sourcePath = Join-Path $repoRoot $entry.Source
    if (-not (Test-Path $sourcePath)) {
        throw "Asset source was not found: $sourcePath"
    }

    $bytes = [System.IO.File]::ReadAllBytes($sourcePath)
    $compressedStream = New-Object System.IO.MemoryStream
    try {
        $deflateStream = New-Object System.IO.Compression.DeflateStream(
            $compressedStream,
            [System.IO.Compression.CompressionLevel]::Optimal,
            $true)
        try {
            $deflateStream.Write($bytes, 0, $bytes.Length)
        }
        finally {
            $deflateStream.Dispose()
        }
        $packedBytes = $compressedStream.ToArray()
    }
    finally {
        $compressedStream.Dispose()
    }
    $records += [pscustomobject]@{
        PackPath = ($entry.PackPath -replace '\\', '/')
        Bytes = $packedBytes
        PackedSize = [UInt64]$packedBytes.Length
        UnpackedSize = [UInt64]$bytes.Length
        Offset = [UInt64]0
    }
}

$headerSize = 8 + 4
$directorySize = 0
foreach ($record in $records) {
    $pathBytes = [System.Text.Encoding]::UTF8.GetBytes($record.PackPath)
    $record | Add-Member -NotePropertyName PathBytes -NotePropertyValue $pathBytes
    $directorySize += 4 + $pathBytes.Length + 8 + 8 + 8
}

$currentOffset = [UInt64]($headerSize + $directorySize)
foreach ($record in $records) {
    $record.Offset = $currentOffset
    $currentOffset += $record.PackedSize
}

$outputDirectory = Split-Path -Parent $OutputPath
if (-not [string]::IsNullOrWhiteSpace($outputDirectory)) {
    New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
}

$stream = [System.IO.File]::Open($OutputPath, [System.IO.FileMode]::Create, [System.IO.FileAccess]::Write)
try {
    $writer = New-Object System.IO.BinaryWriter($stream, [System.Text.Encoding]::UTF8, $true)
    try {
        $writer.Write([System.Text.Encoding]::ASCII.GetBytes("CGPK0002"))
        $writer.Write([UInt32]$records.Count)

        foreach ($record in $records) {
            $writer.Write([UInt32]$record.PathBytes.Length)
            $writer.Write($record.PathBytes)
            $writer.Write([UInt64]$record.Offset)
            $writer.Write([UInt64]$record.PackedSize)
            $writer.Write([UInt64]$record.UnpackedSize)
        }

        foreach ($record in $records) {
            $writer.Write($record.Bytes)
        }
    }
    finally {
        $writer.Dispose()
    }
}
finally {
    $stream.Dispose()
}

Write-Host "Created asset pack: $OutputPath"
