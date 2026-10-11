#include "Application.h"
#include "SoundPlayer.h"

#include <chrono>
#include <thread>

namespace
{
	constexpr auto TargetFrameDuration = std::chrono::duration<double>(1.0 / 60.0);
}

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
	static auto nextFrameTime =
		std::chrono::steady_clock::now() + TargetFrameDuration;

	sceneManager_.Update();
	SoundPlayer::Update();
	renderer_.Draw();
	sceneManager_.Draw();
	renderer_.Swap();

	std::this_thread::sleep_until(nextFrameTime);
	nextFrameTime += TargetFrameDuration;

	const auto now = std::chrono::steady_clock::now();
	if (nextFrameTime < now)
	{
		nextFrameTime = now + TargetFrameDuration;
	}

	return true;
}
