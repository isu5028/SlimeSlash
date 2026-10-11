#pragma once

#include "Component.h"

class RotateComponent : public Component
{
public:
	RotateComponent(GameObject& owner, float rotateSpeed);
	void Update() override;

private:
	float rotateSpeed_;
};