#pragma once
#include "Component.h"

class TransformComponent : public Component
{
public:
	explicit TransformComponent(GameObject& owner)
		:Component(owner)
	{

	}

	DirectX::XMMATRIX GetWorldMatrix() const
	{
		using namespace DirectX;

		const XMMATRIX scale = XMMatrixScaling(
			scale_.x, scale_.y, scale_.z
		);

		const XMMATRIX rotation = XMMatrixRotationRollPitchYaw(
			rotation_.x, rotation_.y, rotation_.z
		);

		const XMMATRIX translation = XMMatrixTranslation(
			position_.x, position_.y, position_.z
		);

		return scale * rotation * translation;
	}

	void SetPosition(const DirectX::XMFLOAT3& position)
	{
		position_ = position;
	}

	void SetRotation(const DirectX::XMFLOAT3& rotation)
	{
		rotation_ = rotation;
	}

	void SetScale(const DirectX::XMFLOAT3& scale)
	{
		scale_ = scale;
	}

	const DirectX::XMFLOAT3& GetPosition() const
	{
		return position_;
	}

	const DirectX::XMFLOAT3& GetRotation() const
	{
		return rotation_;
	}

	const DirectX::XMFLOAT3& GetScale() const
	{
		return scale_;
	}

private:
	DirectX::XMFLOAT3 position_ = { 0.f,0.f,0.f };
	DirectX::XMFLOAT3 rotation_ = { 0.f,0.f,0.f };
	DirectX::XMFLOAT3 scale_ = { 1.f,1.f,1.f };
};