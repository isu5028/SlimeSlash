#include "Camera.h"

#include <algorithm>
#include <cmath>

namespace
{
	float SmoothStep(float t)
	{
		t = std::max(0.0f, std::min(t, 1.0f));
		return t * t * (3.0f - 2.0f * t);
	}

	float InterpolateShake(float progress, float startTime, float endTime, float start, float end)
	{
		const float t = SmoothStep((progress - startTime) / (endTime - startTime));
		return start + (end - start) * t;
	}

	float SampleShakePattern(float progress, bool strongPattern)
	{
		if (!strongPattern)
		{
			if (progress < 0.32f)
				return InterpolateShake(progress, 0.0f, 0.32f, 0.0f, 1.0f);
			if (progress < 0.68f)
				return InterpolateShake(progress, 0.32f, 0.68f, 1.0f, -0.42f);
			return InterpolateShake(progress, 0.68f, 1.0f, -0.42f, 0.0f);
		}

		if (progress < 0.26f)
			return InterpolateShake(progress, 0.0f, 0.26f, 0.0f, 1.0f);
		if (progress < 0.54f)
			return InterpolateShake(progress, 0.26f, 0.54f, 1.0f, -0.62f);
		if (progress < 0.80f)
			return InterpolateShake(progress, 0.54f, 0.80f, -0.62f, 0.28f);
		return InterpolateShake(progress, 0.80f, 1.0f, 0.28f, 0.0f);
	}
}

Camera::Camera()
	: eyePos_(0, 0, -5)
	, focutPos_(0, 0, 0)
{
}

Camera::~Camera()
{
}

void Camera::Update()
{
	const auto now = std::chrono::steady_clock::now();
	const float deltaTime = std::min(
		std::chrono::duration<float>(now - previousUpdate_).count(), 0.05f);
	previousUpdate_ = now;
	if (shakeElapsed_ < shakeDuration_) shakeElapsed_ += deltaTime;

	// 0.03は適当なので環境に合わせて変えてOK
	//eyePos_.y += 0.03f;
	if (eyePos_.y > 10.f) {
		eyePos_.y = 0.f;
	}
}

/*
 * View変換行列取得
 */
DirectX::XMMATRIX Camera::GetViewMatrix() const
{
	XMFLOAT3 shakeOffset = { 0.0f, 0.0f, 0.0f };
	if (shakeElapsed_ < shakeDuration_ && shakeDuration_ > 0.0f)
	{
		const float progress = shakeElapsed_ / shakeDuration_;
		const float pattern = SampleShakePattern(progress, strongShakePattern_);
		shakeOffset.x = shakeDirection_.x * shakeStrength_ * pattern;
		shakeOffset.y = shakeDirection_.y * shakeStrength_ * 0.55f * pattern;
	}
	const XMFLOAT3 shakenEye = {
		eyePos_.x + shakeOffset.x, eyePos_.y + shakeOffset.y, eyePos_.z
	};
	const XMFLOAT3 shakenFocus = {
		focutPos_.x + shakeOffset.x, focutPos_.y + shakeOffset.y, focutPos_.z
	};
	// ビュー変換行列を求める
	XMMATRIX viewMat = XMMatrixLookAtLH(
		XMLoadFloat3(&shakenEye), XMLoadFloat3(&shakenFocus), XMLoadFloat3(&up_));
	return viewMat;
}

void Camera::TriggerShake(
	const XMFLOAT2& worldDirection,
	float strength,
	float duration,
	bool strongPattern)
{
	const float length = std::sqrt(
		worldDirection.x * worldDirection.x + worldDirection.y * worldDirection.y);
	if (length <= 0.0001f || strength <= 0.0f || duration <= 0.0f) return;

	const bool alreadyShaking = shakeElapsed_ < shakeDuration_;
	shakeDirection_ = { worldDirection.x / length, worldDirection.y / length };
	shakeStrength_ = alreadyShaking ? std::max(shakeStrength_, strength) : strength;
	shakeDuration_ = duration;
	shakeElapsed_ = 0.0f;
	strongShakePattern_ = strongPattern;
}
