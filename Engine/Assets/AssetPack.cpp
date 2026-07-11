#include "AssetPack.h"

#include <Windows.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <zlib.h>

namespace
{
    struct AssetEntry
    {
        std::uint64_t offset = 0;
        std::uint64_t packedSize = 0;
        std::uint64_t unpackedSize = 0;
    };

    constexpr char PackMagic[] = "CGPK0002";

    std::unordered_map<std::string, AssetEntry> g_entries;
    std::vector<std::uint8_t> g_packBytes;
    std::wstring g_extractionRoot;
    bool g_initialized = false;
    bool g_available = false;

    std::string NormalizePath(const std::string& path)
    {
        std::string normalized = path;
        std::replace(normalized.begin(), normalized.end(), '\\', '/');
        return normalized;
    }

    std::wstring Utf8ToWide(const std::string& value)
    {
        if (value.empty()) return {};
        const int length = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
        if (length <= 0) return {};
        std::wstring result(static_cast<std::size_t>(length - 1), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, result.data(), length);
        return result;
    }

    std::string WideToUtf8(const std::wstring& value)
    {
        if (value.empty()) return {};
        const int length = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (length <= 0) return {};
        std::string result(static_cast<std::size_t>(length - 1), '\0');
        WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, result.data(), length, nullptr, nullptr);
        return result;
    }

    std::wstring GetExecutableDirectory()
    {
        wchar_t executablePath[MAX_PATH] = {};
        const DWORD length = GetModuleFileNameW(nullptr, executablePath, MAX_PATH);
        if (length == 0 || length >= MAX_PATH) return {};
        return std::filesystem::path(executablePath).parent_path().wstring();
    }

    std::wstring BuildExtractionRoot()
    {
        wchar_t tempPath[MAX_PATH] = {};
        const DWORD length = GetTempPathW(MAX_PATH, tempPath);
        if (length == 0 || length >= MAX_PATH) return {};
        auto root = std::filesystem::path(tempPath) / L"CubeGameAssets" / L"assets-pack";
        return root.wstring();
    }

    bool ReadPack(const std::filesystem::path& packPath, std::string& errorMessage)
    {
        std::ifstream stream(packPath, std::ios::binary | std::ios::ate);
        if (!stream) return false;

        const auto fileSize = stream.tellg();
        if (fileSize < 12)
        {
            errorMessage = "assets.pack is too small";
            return false;
        }

        g_packBytes.resize(static_cast<std::size_t>(fileSize));
        stream.seekg(0);
        if (!stream.read(reinterpret_cast<char*>(g_packBytes.data()), fileSize))
        {
            errorMessage = "Could not read assets.pack";
            return false;
        }

        if (std::memcmp(g_packBytes.data(), PackMagic, 8) != 0)
        {
            errorMessage = "assets.pack has an invalid magic header";
            return false;
        }

        std::size_t position = 8;
        const auto readU32 = [&]() -> std::uint32_t
        {
            const auto value = static_cast<std::uint32_t>(g_packBytes[position]) |
                (static_cast<std::uint32_t>(g_packBytes[position + 1]) << 8) |
                (static_cast<std::uint32_t>(g_packBytes[position + 2]) << 16) |
                (static_cast<std::uint32_t>(g_packBytes[position + 3]) << 24);
            position += 4;
            return value;
        };
        const auto readU64 = [&]() -> std::uint64_t
        {
            std::uint64_t value = 0;
            for (int index = 0; index < 8; ++index)
            {
                value |= static_cast<std::uint64_t>(g_packBytes[position + index]) << (index * 8);
            }
            position += 8;
            return value;
        };

        const std::uint32_t entryCount = readU32();
        g_entries.clear();
        for (std::uint32_t index = 0; index < entryCount; ++index)
        {
            if (position + 4 > g_packBytes.size())
            {
                errorMessage = "assets.pack entry header is truncated";
                return false;
            }

            const std::uint32_t pathLength = readU32();
            if (position + pathLength + 24 > g_packBytes.size())
            {
                errorMessage = "assets.pack entry path is truncated";
                return false;
            }

            std::string path(reinterpret_cast<const char*>(g_packBytes.data() + position), pathLength);
            position += pathLength;

            AssetEntry entry;
            entry.offset = readU64();
            entry.packedSize = readU64();
            entry.unpackedSize = readU64();
            if (entry.offset + entry.packedSize > g_packBytes.size())
            {
                errorMessage = "assets.pack entry points outside the file";
                return false;
            }

            g_entries.emplace(NormalizePath(path), entry);
        }

        return true;
    }

    std::wstring ResolveLogicalPath(const std::string& logicalPath)
    {
        const auto normalized = NormalizePath(logicalPath);
        if (!g_available)
        {
            return Utf8ToWide(normalized);
        }

        const auto found = g_entries.find(normalized);
        if (found == g_entries.end())
        {
            return Utf8ToWide(normalized);
        }

        std::filesystem::path destination(g_extractionRoot);
        destination /= std::filesystem::path(Utf8ToWide(normalized));

        const auto& entry = found->second;
        std::error_code errorCode;
        std::filesystem::create_directories(destination.parent_path(), errorCode);
        if (errorCode)
        {
            return Utf8ToWide(normalized);
        }

        bool shouldWrite = true;
        if (std::filesystem::exists(destination, errorCode) && !errorCode)
        {
            shouldWrite = std::filesystem::file_size(destination, errorCode) != entry.unpackedSize;
            errorCode.clear();
        }

        if (shouldWrite)
        {
            std::vector<std::uint8_t> unpackedBytes(
                static_cast<std::size_t>(entry.unpackedSize));
            z_stream inflateStream = {};
            inflateStream.next_in = const_cast<Bytef*>(
                g_packBytes.data() + static_cast<std::size_t>(entry.offset));
            inflateStream.avail_in = static_cast<uInt>(entry.packedSize);
            inflateStream.next_out = unpackedBytes.data();
            inflateStream.avail_out = static_cast<uInt>(entry.unpackedSize);
            if (inflateInit2(&inflateStream, -MAX_WBITS) != Z_OK)
            {
                return Utf8ToWide(normalized);
            }
            const int inflateResult = inflate(&inflateStream, Z_FINISH);
            inflateEnd(&inflateStream);
            if (inflateResult != Z_STREAM_END ||
                inflateStream.total_out != entry.unpackedSize)
            {
                return Utf8ToWide(normalized);
            }

            std::ofstream output(destination, std::ios::binary | std::ios::trunc);
            if (!output)
            {
                return Utf8ToWide(normalized);
            }
            output.write(
                reinterpret_cast<const char*>(unpackedBytes.data()),
                static_cast<std::streamsize>(unpackedBytes.size()));
        }

        return destination.wstring();
    }
}

bool AssetPack::Initialize(std::string& errorMessage)
{
    errorMessage.clear();
    if (g_initialized) return true;

    g_initialized = true;
    g_available = false;
    g_entries.clear();
    g_packBytes.clear();

    const auto executableDirectory = GetExecutableDirectory();
    if (executableDirectory.empty()) return true;

    const auto packPath = std::filesystem::path(executableDirectory) / L"assets.pack";
    if (!std::filesystem::exists(packPath))
    {
        return true;
    }

    if (!ReadPack(packPath, errorMessage))
    {
        return false;
    }

    g_extractionRoot = BuildExtractionRoot();
    if (g_extractionRoot.empty())
    {
        errorMessage = "Could not determine the temporary extraction directory";
        return false;
    }

    g_available = true;
    return true;
}

std::wstring AssetPack::ResolvePath(const std::wstring& logicalPath)
{
    return ResolveLogicalPath(WideToUtf8(logicalPath));
}

std::wstring AssetPack::ResolvePath(const std::string& logicalPath)
{
    return ResolveLogicalPath(logicalPath);
}

bool AssetPack::IsAvailable()
{
    return g_available;
}
