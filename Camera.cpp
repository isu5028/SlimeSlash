#include "Camera.h"

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
	// 0.03は適当なので環境に合わせて変えてOK
	eyePos_.y += 0.03f;
	if (eyePos_.y > 10.f) {
		eyePos_.y = 0.f;
	}
}

/*
 * View変換行列取得
 */
DirectX::XMMATRIX Camera::GetViewMatrix() const
{
	// ビュー変換行列を求める
	XMMATRIX viewMat = XMMatrixLookAtLH(
		XMLoadFloat3(&eyePos_), XMLoadFloat3(&focutPos_), XMLoadFloat3(&up_));
	return viewMat;
}