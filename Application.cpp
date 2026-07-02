#include "Application.h"

void Application::Initialize(HINSTANCE hInst)
{
	window_.Initialize(hInst);
	renderer_.Initialize(window_.GetWindowHandle());
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
	window_.Terminate();
	renderer_.Terminate();
}

bool Application::gameLoop()
{
	renderer_.Draw();
	renderer_.Swap();
	return true;
}