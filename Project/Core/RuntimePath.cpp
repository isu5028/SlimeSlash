#include "RuntimePath.h"

#include <Windows.h>
#include <string>

namespace
{
    bool IsDirectory(const std::wstring& path)
    {
        const DWORD attributes = GetFileAttributesW(path.c_str());
        return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    }

    bool TryDirectoryAndParents(std::wstring path)
    {
        for (int depth = 0; depth < 8 && !path.empty(); ++depth)
        {
            while (path.size() > 3 && (path.back() == L'\\' || path.back() == L'/')) path.pop_back();
            if (IsDirectory(path + L"\\Project\\Assets") && IsDirectory(path + L"\\Engine\\Shaders"))
                return SetCurrentDirectoryW(path.c_str()) != FALSE;
            const auto separator = path.find_last_of(L"\\/");
            if (separator == std::wstring::npos) break;
            path.resize(separator);
        }
        return false;
    }
}

bool RuntimePath::SetWorkingDirectoryToProjectRoot()
{
    wchar_t currentDirectory[MAX_PATH] = {};
    const DWORD currentLength = GetCurrentDirectoryW(MAX_PATH, currentDirectory);
    if (currentLength > 0 && currentLength < MAX_PATH && TryDirectoryAndParents(currentDirectory))
        return true;

    wchar_t executablePath[MAX_PATH] = {};
    const DWORD executableLength = GetModuleFileNameW(nullptr, executablePath, MAX_PATH);
    if (executableLength == 0 || executableLength >= MAX_PATH) return false;
    std::wstring directory(executablePath, executableLength);
    const auto separator = directory.find_last_of(L"\\/");
    if (separator == std::wstring::npos) return false;
    directory.resize(separator);
    return TryDirectoryAndParents(directory);
}
