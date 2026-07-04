[CmdletBinding()]
param(
    [string]$Configuration = "Release",
    [string]$Platform = "x64",
    [string]$OutputDir = "",
    [switch]$SkipBuild,
    [switch]$IncludePdb
)

$ErrorActionPreference = "Stop"

function Get-MSBuildPath {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) {
        throw "vswhere.exe was not found. Please install Visual Studio with C++ tools."
    }

    $msbuild = & $vswhere -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe |
        Select-Object -First 1

    if ([string]::IsNullOrWhiteSpace($msbuild)) {
        throw "MSBuild.exe was not found. Please check your Visual Studio C++ build tools."
    }

    return $msbuild
}

function Get-DumpbinPath {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) {
        throw "vswhere.exe was not found. Please install Visual Studio with C++ tools."
    }

    $dumpbin = & $vswhere -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -find VC\Tools\MSVC\**\bin\Hostx64\x64\dumpbin.exe |
        Select-Object -First 1

    if ([string]::IsNullOrWhiteSpace($dumpbin)) {
        throw "dumpbin.exe was not found. Please check your Visual Studio C++ tools."
    }

    return $dumpbin
}

$scriptDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Split-Path -Parent $scriptDirectory

if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $OutputDir = Join-Path $repoRoot "dist\CubeGame"
}

$solutionPath = Join-Path $repoRoot "CubeGame.sln"
$buildOutputDir = Join-Path $repoRoot "$Platform\$Configuration"

if (-not (Test-Path $solutionPath)) {
    throw "Solution file was not found: $solutionPath"
}

if (-not $SkipBuild) {
    $msbuild = Get-MSBuildPath
    Write-Host "Building $Configuration|$Platform with MSBuild..."
    & $msbuild $solutionPath /t:Build /p:Configuration=$Configuration /p:Platform=$Platform /m
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed."
    }
}

$exePath = Join-Path $buildOutputDir "CubeGame.exe"
if (-not (Test-Path $exePath)) {
    throw "Required build output was not found: $exePath"
}

if (Test-Path $OutputDir) {
    Remove-Item -LiteralPath $OutputDir -Recurse -Force
}

New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
Copy-Item -Path $exePath -Destination $OutputDir -Force

$dumpbin = Get-DumpbinPath
$dependencies = & $dumpbin /dependents $exePath
if ($LASTEXITCODE -ne 0) {
    throw "Failed to inspect executable dependencies."
}

$copiedRuntimeFiles = @("CubeGame.exe")

if ($dependencies -match 'assimp-vc142-mt\.dll') {
    $assimpDllPath = Join-Path $buildOutputDir "assimp-vc142-mt.dll"
    if (-not (Test-Path $assimpDllPath)) {
        throw "CubeGame.exe depends on assimp-vc142-mt.dll, but the DLL was not found."
    }

    Copy-Item -Path $assimpDllPath -Destination $OutputDir -Force
    $copiedRuntimeFiles += "assimp-vc142-mt.dll"
}

if ($IncludePdb) {
    $pdbPath = Join-Path $buildOutputDir "CubeGame.pdb"
    if (Test-Path $pdbPath) {
        Copy-Item -Path $pdbPath -Destination $OutputDir -Force
        $copiedRuntimeFiles += "CubeGame.pdb"
    }
}

$assetPackScript = Join-Path $scriptDirectory "Create-AssetPack.ps1"
powershell -ExecutionPolicy Bypass -File $assetPackScript -OutputPath (Join-Path $OutputDir "assets.pack")
if ($LASTEXITCODE -ne 0) {
    throw "Asset pack generation failed."
}

$summary = @(
    "Distribution folder created:",
    "  $OutputDir",
    "",
    "Runtime files:"
)

foreach ($runtimeFile in $copiedRuntimeFiles) {
    $summary += "  $runtimeFile"
}

$summary += @(
    "  assets.pack"
)

Write-Host ($summary -join [Environment]::NewLine)
