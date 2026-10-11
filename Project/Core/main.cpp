#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <objbase.h>

#include <string>

#include "Application.h"
#include "AssetPack.h"
#include "RuntimePath.h"
#include "SoundPlayer.h"

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int)
{
    const HRESULT comResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    Application* application = new Application();
    std::string assetPackError;
    if (!RuntimePath::SetWorkingDirectoryToProjectRoot() ||
        !AssetPack::Initialize(assetPackError) ||
        !SoundPlayer::Initialize() ||
        !application->Initialize(hInst))
    {
        MessageBoxW(
            nullptr,
            L"assets.pack or the raw asset folders could not be loaded, or Direct3D initialization failed.",
            L"Slime Slash Slayer initialization error",
            MB_OK | MB_ICONERROR);
        application->Terminate();
        SoundPlayer::Terminate();
        delete application;
        if (SUCCEEDED(comResult)) CoUninitialize();
        return 1;
    }

    application->Loop();
    application->Terminate();
    SoundPlayer::Terminate();
    delete application;
    if (SUCCEEDED(comResult)) CoUninitialize();
    return 0;
}
