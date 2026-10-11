#include "RotateComponent.h"
#include "GameObject.h"
#include "TransformComponent.h"

RotateComponent::RotateComponent(
	GameObject& owner,
	float rotateSpeed
)
	: Component(owner)
	, rotateSpeed_(rotateSpeed)
{

}

void RotateComponent::Update()
{
	auto& transform = GetOwner().GetTransform();
	auto rotation = transform.GetRotation();
	rotation.y += rotateSpeed_;

	transform.SetRotation(rotation);
}