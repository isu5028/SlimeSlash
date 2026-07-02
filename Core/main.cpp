#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include "Application.h"

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int)
{
	Application* application = new Application();
	// アプリ初期化
	application->Initialize(hInst);

	// ループ：アプリメインの実装を書くところ
	application->Loop();

	// アプリ終了処理
	application->Terminate();

	delete(application);

	return 0;
}
