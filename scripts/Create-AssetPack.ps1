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
    @{ Source = "Assets\Images\aseprite\Player.aseprite"; PackPath = "Assets/Images/aseprite/Player.aseprite" },
    @{ Source = "Assets\Images\aseprite\Slime.aseprite"; PackPath = "Assets/Images/aseprite/Slime.aseprite" },
    @{ Source = "Assets\Images\aseprite\HealthBar.aseprite"; PackPath = "Assets/Images/aseprite/HealthBar.aseprite" },
    @{ Source = "Project\Assets\Sounds\Dash.wav"; PackPath = "Project/Assets/Sounds/Dash.wav" },
    @{ Source = "Project\Assets\Sounds\Jump.wav"; PackPath = "Project/Assets/Sounds/Jump.wav" },
    @{ Source = "Project\Assets\Sounds\PlayerAttackHit.wav"; PackPath = "Project/Assets/Sounds/PlayerAttackHit.wav" },
    @{ Source = "Project\Assets\Sounds\PlayerAttackMiss.wav"; PackPath = "Project/Assets/Sounds/PlayerAttackMiss.wav" },
    @{ Source = "Project\Assets\Sounds\PlayerDamaged.wav"; PackPath = "Project/Assets/Sounds/PlayerDamaged.wav" },
    @{ Source = "Engine\Shaders\VertexShader.hlsl"; PackPath = "Engine/Shaders/VertexShader.hlsl" },
    @{ Source = "Engine\Shaders\PixelShader.hlsl"; PackPath = "Engine/Shaders/PixelShader.hlsl" },
    @{ Source = "Engine\Shaders\SpriteVertexShader.hlsl"; PackPath = "Engine/Shaders/SpriteVertexShader.hlsl" },
    @{ Source = "Engine\Shaders\SpritePixelShader.hlsl"; PackPath = "Engine/Shaders/SpritePixelShader.hlsl" },
    @{ Source = "Engine\Shaders\ColoredPlaneDitherShader.hlsl"; PackPath = "Engine/Shaders/ColoredPlaneDitherShader.hlsl" }
)

$records = @()
foreach ($entry in $assetEntries) {
    $sourcePath = Join-Path $repoRoot $entry.Source
    if (-not (Test-Path $sourcePath)) {
        throw "Asset source was not found: $sourcePath"
    }

    $bytes = [System.IO.File]::ReadAllBytes($sourcePath)
    $records += [pscustomobject]@{
        PackPath = ($entry.PackPath -replace '\\', '/')
        Bytes = $bytes
        Size = [UInt64]$bytes.Length
        Offset = [UInt64]0
    }
}

$headerSize = 8 + 4
$directorySize = 0
foreach ($record in $records) {
    $pathBytes = [System.Text.Encoding]::UTF8.GetBytes($record.PackPath)
    $record | Add-Member -NotePropertyName PathBytes -NotePropertyValue $pathBytes
    $directorySize += 4 + $pathBytes.Length + 8 + 8
}

$currentOffset = [UInt64]($headerSize + $directorySize)
foreach ($record in $records) {
    $record.Offset = $currentOffset
    $currentOffset += $record.Size
}

$outputDirectory = Split-Path -Parent $OutputPath
if (-not [string]::IsNullOrWhiteSpace($outputDirectory)) {
    New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
}

$stream = [System.IO.File]::Open($OutputPath, [System.IO.FileMode]::Create, [System.IO.FileAccess]::Write)
try {
    $writer = New-Object System.IO.BinaryWriter($stream, [System.Text.Encoding]::UTF8, $true)
    try {
        $writer.Write([System.Text.Encoding]::ASCII.GetBytes("CGPK0001"))
        $writer.Write([UInt32]$records.Count)

        foreach ($record in $records) {
            $writer.Write([UInt32]$record.PathBytes.Length)
            $writer.Write($record.PathBytes)
            $writer.Write([UInt64]$record.Offset)
            $writer.Write([UInt64]$record.Size)
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
