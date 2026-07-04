#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>

#include <string>

#include "Application.h"
#include "AssetPack.h"
#include "RuntimePath.h"

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int)
{
    Application* application = new Application();
    std::string assetPackError;
    if (!RuntimePath::SetWorkingDirectoryToProjectRoot() ||
        !AssetPack::Initialize(assetPackError) ||
        !application->Initialize(hInst))
    {
        MessageBoxW(
            nullptr,
            L"assets.pack or the raw asset folders could not be loaded, or Direct3D initialization failed.",
            L"CubeGame initialization error",
            MB_OK | MB_ICONERROR);
        application->Terminate();
        delete application;
        return 1;
    }

    application->Loop();
    application->Terminate();
    delete application;
    return 0;
}
