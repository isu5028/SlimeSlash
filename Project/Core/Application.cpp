#include "Application.h"

bool Application::Initialize(HINSTANCE hInst)
{
	if (!window_.Initialize(hInst)) return false;
	if (!renderer_.Initialize(window_.GetWindowHandle())) return false;
	sceneManager_.Initialize(renderer_);
	return true;
}

void Application::Loop()
{
	while (true) {
		bool result = window_.MessageLoop();
		if (result == false) break;

		result = gameLoop();
		if (result == false) break;
	}
}

void Application::Terminate()
{
	sceneManager_.Terminate();
	window_.Terminate();
	renderer_.Terminate();
}

bool Application::gameLoop()
{
	sceneManager_.Update();
	renderer_.Draw();
	sceneManager_.Draw();
	renderer_.Swap();

	Sleep(10);
	return true;
}
