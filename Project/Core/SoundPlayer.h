#pragma once

class SoundPlayer
{
public:
    static bool Initialize();
    static void Terminate();
    static bool Play(const wchar_t* filePath);
    static bool PlayBgm(const wchar_t* filePath, bool loop = true);
    static void Update();
    static void Stop();
    static void StopBgm();
    static void SetMasterVolume(float volume);
    static void SetBgmVolume(float volume);
    static void SetSeVolume(float volume);
    static float GetMasterVolume();
    static float GetBgmVolume();
    static float GetSeVolume();
};
