#pragma once

class SoundPlayer
{
public:
    static bool Play(const wchar_t* filePath);
    static void Stop();
};
