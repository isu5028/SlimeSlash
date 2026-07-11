#pragma once

#include <string>

namespace AssetPack
{
    bool Initialize(std::string& errorMessage);
    std::wstring ResolvePath(const std::wstring& logicalPath);
    std::wstring ResolvePath(const std::string& logicalPath);
    bool IsAvailable();
}
