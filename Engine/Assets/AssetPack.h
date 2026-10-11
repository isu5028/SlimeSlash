#pragma once

#include <string>

namespace AssetPack
{
    // 実行ファイルの隣にassets.packがあれば索引を読み込む
    bool Initialize(std::string& errorMessage);
    // 論理パスを、生ファイルまたは一時展開先のパスへ解決する
    std::wstring ResolvePath(const std::wstring& logicalPath);
    std::wstring ResolvePath(const std::string& logicalPath);
    bool IsAvailable();
}
