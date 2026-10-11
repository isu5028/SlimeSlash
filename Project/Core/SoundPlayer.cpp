#include "SoundPlayer.h"
#include "AssetPack.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

#include <Windows.h>
#include <dshow.h>

#pragma comment(lib, "strmiids.lib")

namespace
{
    constexpr std::size_t MaxSimultaneousSeCount = 3;
    constexpr std::array<const wchar_t*, 14> SePaths =
    {
        L"Project/Assets/Sounds/Dash.wav",
        L"Project/Assets/Sounds/Jump.wav",
        L"Project/Assets/Sounds/PlayerAttackHit.wav",
        L"Project/Assets/Sounds/PlayerAttackHit2.wav",
        L"Project/Assets/Sounds/PlayerAttackHit3.wav",
        L"Project/Assets/Sounds/PlayerAttackMiss.wav",
        L"Project/Assets/Sounds/PlayerAttackMiss2.wav",
        L"Project/Assets/Sounds/PlayerAttackMiss3.wav",
        L"Project/Assets/Sounds/PlayerDamaged.wav",
        L"Project/Assets/Sounds/SlimeExplode.wav",
        L"Project/Assets/Sounds/SlimeFloor.wav",
        L"Project/Assets/Sounds/SlimeJump.wav",
        L"Project/Assets/Sounds/SlimeRush.wav",
        L"Project/Assets/Sounds/SlimeStamp.wav"
    };
    constexpr std::size_t SePathCount = SePaths.size();

    struct SeSlot
    {
        bool active = false;
        std::size_t clipIndex = SePathCount;
    };

    struct SePlayback
    {
        IGraphBuilder* graph = nullptr;
        IMediaControl* control = nullptr;
        IMediaSeeking* seeking = nullptr;
        IBasicAudio* audio = nullptr;
        bool loaded = false;
    };

    struct SeClip
    {
        std::wstring logicalPath;
        std::array<SePlayback, MaxSimultaneousSeCount> playbacks;
    };

    std::array<SeSlot, MaxSimultaneousSeCount> g_seSlots;
    std::array<SeClip, SePathCount> g_seClips;
    std::size_t g_nextSeSlotIndex = 0;
    bool g_seInitialized = false;
    float g_masterVolume = 1.0f;
    float g_bgmVolume = 1.0f;
    float g_seVolume = 1.0f;
    bool g_bgmLoop = false;
    IGraphBuilder* g_bgmGraph = nullptr;
    IMediaControl* g_bgmControl = nullptr;
    IMediaEventEx* g_bgmEvent = nullptr;
    IMediaSeeking* g_bgmSeeking = nullptr;
    IBasicAudio* g_bgmAudio = nullptr;

    float ClampVolume(float volume)
    {
        return std::max(0.0f, std::min(volume, 1.0f));
    }

    long ToDirectShowVolume(float volume)
    {
        const float clamped = ClampVolume(volume);
        if (clamped <= 0.0001f) return -10000;
        const double decibels = 2000.0 * std::log10(clamped);
        return static_cast<long>(std::max(-10000.0, std::min(decibels, 0.0)));
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

    void ReleaseSePlayback(SePlayback& playback)
    {
        if (playback.control) playback.control->Stop();
        SafeRelease(playback.audio);
        SafeRelease(playback.seeking);
        SafeRelease(playback.control);
        SafeRelease(playback.graph);
        playback.loaded = false;
    }

    void ApplySeVolume()
    {
        const long volume = ToDirectShowVolume(g_masterVolume * g_seVolume);
        for (const SeClip& clip : g_seClips)
        {
            for (const SePlayback& playback : clip.playbacks)
            {
                if (!playback.loaded || !playback.audio) continue;
                playback.audio->put_Volume(volume);
            }
        }
    }

    void ApplyBgmVolume()
    {
        if (!g_bgmAudio) return;
        g_bgmAudio->put_Volume(ToDirectShowVolume(g_masterVolume * g_bgmVolume));
    }

    void InitializeSePlayback(const std::wstring& resolvedPath, SePlayback& playback)
    {
        ReleaseSePlayback(playback);

        HRESULT result = CoCreateInstance(
            CLSID_FilterGraph, nullptr, CLSCTX_INPROC_SERVER,
            IID_IGraphBuilder, reinterpret_cast<void**>(&playback.graph));
        if (FAILED(result))
        {
            ReleaseSePlayback(playback);
            return;
        }

        result = playback.graph->QueryInterface(
            IID_IMediaControl, reinterpret_cast<void**>(&playback.control));
        if (FAILED(result))
        {
            ReleaseSePlayback(playback);
            return;
        }

        result = playback.graph->QueryInterface(
            IID_IMediaSeeking, reinterpret_cast<void**>(&playback.seeking));
        if (FAILED(result))
        {
            ReleaseSePlayback(playback);
            return;
        }

        result = playback.graph->QueryInterface(
            IID_IBasicAudio, reinterpret_cast<void**>(&playback.audio));
        if (FAILED(result))
        {
            ReleaseSePlayback(playback);
            return;
        }

        result = playback.graph->RenderFile(resolvedPath.c_str(), nullptr);
        if (FAILED(result))
        {
            ReleaseSePlayback(playback);
            return;
        }

        playback.loaded = true;
    }

    void StopSeSlot(std::size_t slotIndex)
    {
        if (slotIndex >= MaxSimultaneousSeCount) return;

        SeSlot& slot = g_seSlots[slotIndex];
        if (slot.active && slot.clipIndex < SePathCount &&
            g_seClips[slot.clipIndex].playbacks[slotIndex].loaded)
        {
            IMediaControl* control = g_seClips[slot.clipIndex].playbacks[slotIndex].control;
            if (control) control->Stop();
        }
        g_seSlots[slotIndex].active = false;
        g_seSlots[slotIndex].clipIndex = SePathCount;
    }

    void RefreshSeSlots()
    {
        for (std::size_t i = 0; i < MaxSimultaneousSeCount; ++i)
        {
            if (!g_seSlots[i].active) continue;

            const SeSlot& slot = g_seSlots[i];
            if (slot.clipIndex >= SePathCount ||
                !g_seClips[slot.clipIndex].playbacks[i].loaded)
            {
                g_seSlots[i].active = false;
                g_seSlots[i].clipIndex = SePathCount;
                continue;
            }

            OAFilterState state = State_Stopped;
            IMediaControl* control = g_seClips[slot.clipIndex].playbacks[i].control;
            if (!control || FAILED(control->GetState(0, &state)))
            {
                g_seSlots[i].active = false;
                g_seSlots[i].clipIndex = SePathCount;
                continue;
            }

            if (state == State_Stopped)
            {
                g_seSlots[i].active = false;
                g_seSlots[i].clipIndex = SePathCount;
            }
        }
    }

    std::wstring NormalizeLogicalPath(const wchar_t* filePath)
    {
        std::wstring normalized(filePath ? filePath : L"");
        std::replace(normalized.begin(), normalized.end(), L'\\', L'/');
        return normalized;
    }

    std::size_t FindSeClip(const wchar_t* filePath)
    {
        const std::wstring normalizedPath = NormalizeLogicalPath(filePath);
        for (std::size_t clipIndex = 0; clipIndex < SePathCount; ++clipIndex)
        {
            if (g_seClips[clipIndex].logicalPath == normalizedPath)
            {
                return clipIndex;
            }
        }
        return SePathCount;
    }

}

bool SoundPlayer::Initialize()
{
    if (g_seInitialized) return true;

    g_seInitialized = true;
    g_nextSeSlotIndex = 0;
    for (SeSlot& slot : g_seSlots)
    {
        slot.active = false;
        slot.clipIndex = SePathCount;
    }

    for (std::size_t clipIndex = 0; clipIndex < SePathCount; ++clipIndex)
    {
        SeClip& clip = g_seClips[clipIndex];
        clip.logicalPath = SePaths[clipIndex];

        const std::wstring resolvedPath = AssetPack::ResolvePath(clip.logicalPath);
        for (std::size_t slotIndex = 0; slotIndex < MaxSimultaneousSeCount; ++slotIndex)
        {
            InitializeSePlayback(resolvedPath, clip.playbacks[slotIndex]);
        }
    }

    ApplySeVolume();
    return true;
}

void SoundPlayer::Terminate()
{
    Stop();
    StopBgm();

    for (SeClip& clip : g_seClips)
    {
        for (SePlayback& playback : clip.playbacks)
        {
            ReleaseSePlayback(playback);
        }
        clip.logicalPath.clear();
    }

    g_seInitialized = false;
    g_nextSeSlotIndex = 0;
}

bool SoundPlayer::Play(const wchar_t* filePath)
{
    if (!filePath || filePath[0] == L'\0') return false;
    if (!g_seInitialized && !Initialize()) return false;
    if (g_masterVolume <= 0.0f || g_seVolume <= 0.0f) return true;

    const std::size_t clipIndex = FindSeClip(filePath);
    if (clipIndex >= SePathCount) return false;

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

    SePlayback& playback = g_seClips[clipIndex].playbacks[slotIndex];
    if (!playback.loaded || !playback.control || !playback.seeking) return false;

    LONGLONG startPosition = 0;
    if (FAILED(playback.seeking->SetPositions(
        &startPosition, AM_SEEKING_AbsolutePositioning,
        nullptr, AM_SEEKING_NoPositioning)))
    {
        return false;
    }

    if (FAILED(playback.control->Run())) return false;

    g_seSlots[slotIndex].active = true;
    g_seSlots[slotIndex].clipIndex = clipIndex;
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
