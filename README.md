# SlimeSlashSlayer

DirectX 11 を使った Windows 向けゲームプロジェクトです。

## 他の PC でのセットアップ

### 必要な環境

- Windows 10 または Windows 11
- Visual Studio 2022
- Visual Studio Installer で次を選択
  - **C++ によるデスクトップ開発** ワークロード
  - MSVC v143 C++ x64/x86 ビルドツール
  - Windows 10 SDK
- NuGet パッケージを復元できるインターネット接続

### クローンしてビルド

PowerShell で実行します。

```powershell
git clone https://github.com/isu5028/DirectX11Practice.git
cd DirectX11Practice
nuget restore .\CubeGame.sln
msbuild .\CubeGame.sln /p:Configuration=Debug /p:Platform=x64
```

`nuget` コマンドが見つからない場合は、Visual Studio で `CubeGame.sln` を開き、ソリューションを右クリックして **NuGet パッケージの復元** を実行してください。その後、構成を `Debug`、プラットフォームを `x64` にしてビルドします。NuGet の `AssimpCpp` は `packages.config` に記載されています。

Release ビルドは次のコマンドで行えます。

```powershell
msbuild .\CubeGame.sln /p:Configuration=Release /p:Platform=x64
```

実行ファイルは `x64\Debug` または `x64\Release` に生成されます。配布用フォルダを作る方法は [DISTRIBUTION.md](DISTRIBUTION.md) を参照してください。

## プロジェクト構成

- `Engine/` — DirectX 11 エンジンコードとシェーダー
- `Project/` — ゲームコードとゲーム用アセット
- `Assets/` — 画像・スプライト素材
- `ThirdParty/zlib/` — 同梱 zlib ソース
- `CubeGame.sln` — Visual Studio ソリューション
