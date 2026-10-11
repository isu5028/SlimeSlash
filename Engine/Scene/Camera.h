#pragma once

#include <chrono>

class Camera
{
public:
	Camera();
	~Camera();

	void Update();
	void SetEyePosition(const XMFLOAT3& position) { eyePos_ = position; }
	void SetFocusPosition(const XMFLOAT3& position) { focutPos_ = position; }
	const XMFLOAT3& GetEyePosition() const { return eyePos_; }
	const XMFLOAT3& GetFocusPosition() const { return focutPos_; }
	void TriggerShake(
		const XMFLOAT2& worldDirection,
		float strength,
		float duration,
		bool strongPattern = false);

	XMMATRIX GetViewMatrix() const;

private:
	XMFLOAT3 eyePos_ = { 0,0,0 };
	XMFLOAT3 focutPos_ = { 0,0,0 };
	XMFLOAT3 up_ = { 0,1.f,0 };
	XMFLOAT2 shakeDirection_ = { 0.0f, 0.0f };
	float shakeStrength_ = 0.0f;
	float shakeDuration_ = 0.0f;
	float shakeElapsed_ = 0.0f;
	bool strongShakePattern_ = false;
	std::chrono::steady_clock::time_point previousUpdate_ = std::chrono::steady_clock::now();
};
