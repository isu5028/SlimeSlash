#include "SoundPlayer.h"
#include "AssetPack.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

#include <Windows.h>
#include <dshow.h>
#include <mmsystem.h>

#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "strmiids.lib")

namespace
{
    constexpr std::size_t MaxSimultaneousSeCount = 3;
    constexpr std::array<const wchar_t*, MaxSimultaneousSeCount> SeAliases =
    {
        L"CubeGameSe0",
        L"CubeGameSe1",
        L"CubeGameSe2"
    };

    struct SeSlot
    {
        bool active = false;
    };

    std::array<SeSlot, MaxSimultaneousSeCount> g_seSlots;
    std::size_t g_nextSeSlotIndex = 0;
    float g_masterVolume = 1.0f;
    float g_bgmVolume = 1.0f;
    float g_seVolume = 1.0f;
    bool g_bgmLoop = false;
    IGraphBuilder* g_bgmGraph = nullptr;
    IMediaControl* g_bgmControl = nullptr;
    IMediaEventEx* g_bgmEvent = nullptr;
    IMediaSeeking* g_bgmSeeking = nullptr;
    IBasicAudio* g_bgmAudio = nullptr;

    int ToMciVolume(float volume)
    {
        const float clamped = std::max(0.0f, std::min(volume, 1.0f));
        return static_cast<int>(clamped * 1000.0f + 0.5f);
    }

    float ClampVolume(float volume)
    {
        return std::max(0.0f, std::min(volume, 1.0f));
    }

    std::wstring EscapeMciString(const std::wstring& value)
    {
        std::wstring escaped;
        escaped.reserve(value.size());
        for (const wchar_t character : value)
        {
            if (character == L'"')
            {
                escaped += L"\"\"";
            }
            else
            {
                escaped += character;
            }
        }
        return escaped;
    }

    bool SendMciCommand(const std::wstring& command)
    {
        return mciSendStringW(command.c_str(), nullptr, 0, nullptr) == 0;
    }

    bool QueryMciString(const std::wstring& command, wchar_t* buffer, std::size_t bufferCount)
    {
        return mciSendStringW(command.c_str(), buffer, static_cast<UINT>(bufferCount), nullptr) == 0;
    }

    long ToDirectShowVolume(float volume)
    {
        const float clamped = ClampVolume(volume);
        if (clamped <= 0.0001f) return -10000;
        const double decibels = 2000.0 * std::log10(clamped);
        return static_cast<long>(std::max(-10000.0, std::min(decibels, 0.0)));
    }

    void ApplySeVolume()
    {
        waveOutSetVolume(
            nullptr,
            MAKELONG(ToMciVolume(g_masterVolume * g_seVolume) * 65,
                     ToMciVolume(g_masterVolume * g_seVolume) * 65));
    }

    void ApplyBgmVolume()
    {
        if (!g_bgmAudio) return;
        g_bgmAudio->put_Volume(ToDirectShowVolume(g_masterVolume * g_bgmVolume));
    }

    template <typename T>
    void SafeRelease(T*& pointer)
    {
        if (pointer)
        {
            pointer->Release();
            pointer = nullptr;
        }
    }

    void StopSeSlot(std::size_t slotIndex)
    {
        const wchar_t* alias = SeAliases[slotIndex];
        SendMciCommand(std::wstring(L"stop ") + alias);
        SendMciCommand(std::wstring(L"close ") + alias);
        g_seSlots[slotIndex].active = false;
    }

    void RefreshSeSlots()
    {
        std::array<wchar_t, 32> modeBuffer{};
        for (std::size_t i = 0; i < MaxSimultaneousSeCount; ++i)
        {
            if (!g_seSlots[i].active) continue;

            if (!QueryMciString(std::wstring(L"status ") + SeAliases[i] + L" mode",
                modeBuffer.data(), modeBuffer.size()))
            {
                g_seSlots[i].active = false;
                continue;
            }

            if (wcscmp(modeBuffer.data(), L"stopped") == 0)
            {
                StopSeSlot(i);
            }
        }
    }
}

bool SoundPlayer::Play(const wchar_t* filePath)
{
    if (!filePath || filePath[0] == L'\0') return false;
    if (g_masterVolume <= 0.0f || g_seVolume <= 0.0f) return true;

    const std::wstring resolvedPath = AssetPack::ResolvePath(filePath);
    const std::wstring escapedPath = EscapeMciString(resolvedPath);

    RefreshSeSlots();

    std::size_t slotIndex = MaxSimultaneousSeCount;
    for (std::size_t i = 0; i < MaxSimultaneousSeCount; ++i)
    {
        if (g_seSlots[i].active) continue;
        slotIndex = i;
        break;
    }

    if (slotIndex == MaxSimultaneousSeCount)
    {
        slotIndex = g_nextSeSlotIndex;
        StopSeSlot(slotIndex);
    }

    const wchar_t* alias = SeAliases[slotIndex];

    if (!SendMciCommand(
        L"open \"" + escapedPath + L"\" type waveaudio alias " + alias))
    {
        return false;
    }

    ApplySeVolume();
    if (!SendMciCommand(std::wstring(L"play ") + alias))
    {
        StopSeSlot(slotIndex);
        return false;
    }

    g_seSlots[slotIndex].active = true;
    g_nextSeSlotIndex = (slotIndex + 1) % MaxSimultaneousSeCount;
    return true;
}

bool SoundPlayer::PlayBgm(const wchar_t* filePath, bool loop)
{
    if (!filePath || filePath[0] == L'\0') return false;

    const std::wstring resolvedPath = AssetPack::ResolvePath(filePath);
    StopBgm();

    HRESULT result = CoCreateInstance(
        CLSID_FilterGraph, nullptr, CLSCTX_INPROC_SERVER,
        IID_IGraphBuilder, reinterpret_cast<void**>(&g_bgmGraph));
    if (FAILED(result)) return false;

    result = g_bgmGraph->QueryInterface(IID_IMediaControl, reinterpret_cast<void**>(&g_bgmControl));
    if (FAILED(result)) { StopBgm(); return false; }
    result = g_bgmGraph->QueryInterface(IID_IMediaEventEx, reinterpret_cast<void**>(&g_bgmEvent));
    if (FAILED(result)) { StopBgm(); return false; }
    result = g_bgmGraph->QueryInterface(IID_IMediaSeeking, reinterpret_cast<void**>(&g_bgmSeeking));
    if (FAILED(result)) { StopBgm(); return false; }
    result = g_bgmGraph->QueryInterface(IID_IBasicAudio, reinterpret_cast<void**>(&g_bgmAudio));
    if (FAILED(result)) { StopBgm(); return false; }

    result = g_bgmGraph->RenderFile(resolvedPath.c_str(), nullptr);
    if (FAILED(result)) { StopBgm(); return false; }

    ApplyBgmVolume();
    result = g_bgmControl->Run();
    if (FAILED(result)) { StopBgm(); return false; }

    g_bgmLoop = loop;
    return true;
}

void SoundPlayer::Update()
{
    RefreshSeSlots();

    if (!g_bgmLoop || !g_bgmEvent || !g_bgmSeeking || !g_bgmControl) return;

    long eventCode = 0;
    LONG_PTR param1 = 0;
    LONG_PTR param2 = 0;
    while (SUCCEEDED(g_bgmEvent->GetEvent(&eventCode, &param1, &param2, 0)))
    {
        g_bgmEvent->FreeEventParams(eventCode, param1, param2);
        if (eventCode != EC_COMPLETE) continue;

        LONGLONG startPosition = 0;
        g_bgmSeeking->SetPositions(
            &startPosition, AM_SEEKING_AbsolutePositioning,
            nullptr, AM_SEEKING_NoPositioning);
        g_bgmControl->Run();
    }
}

void SoundPlayer::Stop()
{
    for (std::size_t i = 0; i < MaxSimultaneousSeCount; ++i)
    {
        StopSeSlot(i);
    }
}

void SoundPlayer::StopBgm()
{
    g_bgmLoop = false;
    if (g_bgmControl) g_bgmControl->Stop();
    SafeRelease(g_bgmAudio);
    SafeRelease(g_bgmSeeking);
    SafeRelease(g_bgmEvent);
    SafeRelease(g_bgmControl);
    SafeRelease(g_bgmGraph);
}

void SoundPlayer::SetMasterVolume(float volume)
{
    g_masterVolume = ClampVolume(volume);
    ApplyBgmVolume();
    ApplySeVolume();
}

void SoundPlayer::SetBgmVolume(float volume)
{
    g_bgmVolume = ClampVolume(volume);
    ApplyBgmVolume();
}

void SoundPlayer::SetSeVolume(float volume)
{
    g_seVolume = ClampVolume(volume);
    ApplySeVolume();
}

float SoundPlayer::GetMasterVolume()
{
    return g_masterVolume;
}

float SoundPlayer::GetBgmVolume()
{
    return g_bgmVolume;
}

float SoundPlayer::GetSeVolume()
{
    return g_seVolume;
}
