#include "SoundPlayer.h"
#include "AssetPack.h"

#include <Windows.h>
#include <mmsystem.h>

#pragma comment(lib, "winmm.lib")

bool SoundPlayer::Play(const wchar_t* filePath)
{
    if (!filePath || filePath[0] == L'\0') return false;
    const std::wstring resolvedPath = AssetPack::ResolvePath(filePath);
    return PlaySoundW(resolvedPath.c_str(), nullptr,
        SND_FILENAME | SND_ASYNC | SND_NODEFAULT) != FALSE;
}

void SoundPlayer::Stop()
{
    PlaySoundW(nullptr, nullptr, 0);
}
