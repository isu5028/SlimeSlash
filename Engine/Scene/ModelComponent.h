#pragma once

#include "Component.h"
#include "Model.h"

class ModelComponent : public Component
{
public:
	explicit ModelComponent(GameObject& owner);
	~ModelComponent() override;

	bool Setup(Renderer& renderer, const char* filePath);
	void Draw(Renderer& renderer) override;

private:
	void SetupWorldTransform(Renderer& renderer);
	Model model_;
};