#pragma once

#include "Camera.h"

class SceneManager
{
public:
	SceneManager();
	~SceneManager();

	void Initialize(Renderer& renderer);
	void Terminate();

	void Update();
	void Draw();
private:
	Renderer* pRenderer_ = nullptr;
	Camera camera_;
	Triangle sampleTriangle_;
	Model sampleModel_;
};