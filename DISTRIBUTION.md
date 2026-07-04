# Distribution Build

Use `scripts/Create-Distribution.ps1` to generate a distributable folder.

## Command

Run this from the repository root:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\Create-Distribution.ps1
```

This creates `dist/CubeGame` and copies:

- `CubeGame.exe`
- `assets.pack`

If `CubeGame.exe` still depends on `assimp-vc142-mt.dll`, the script also copies that DLL automatically.

## Common options

Skip the build and package the current `x64/Release` output:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\Create-Distribution.ps1 -SkipBuild
```

Include the PDB file:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\Create-Distribution.ps1 -IncludePdb
```

Change the output directory:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\Create-Distribution.ps1 -OutputDir .\dist\CubeGame-test
```

## What the script does

1. Builds `Release|x64` with `MSBuild` unless `-SkipBuild` is specified.
2. Copies `CubeGame.exe` from `x64/Release`.
3. Checks the executable dependencies and copies `assimp-vc142-mt.dll` only when it is actually required.
4. Builds `assets.pack` from the aseprite files, shader files, and sound files used by the game.

## Runtime behavior

- In the distribution build, the game reads from `assets.pack`.
- In the development repository, the game still falls back to the raw asset files when `assets.pack` is not present next to the executable.
