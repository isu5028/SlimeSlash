#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include "Application.h"
#include "RuntimePath.h"

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int)
{
	Application* application = new Application();
	// アプリ初期化
	if (!RuntimePath::SetWorkingDirectoryToProjectRoot() || !application->Initialize(hInst))
	{
		MessageBoxW(nullptr,
			L"Project/Assets、Engine/Shaders、またはDirect3Dの初期化に失敗しました。",
			L"CubeGame initialization error", MB_OK | MB_ICONERROR);
		application->Terminate();
		delete application;
		return 1;
	}

	// ループ：アプリメインの実装を書くところ
	application->Loop();

	// アプリ終了処理
	application->Terminate();

	delete(application);

	return 0;
}
