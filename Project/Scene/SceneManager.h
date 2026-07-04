#pragma once

#include <memory>
#include "Camera.h"
#include "GameObject.h"

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
	
	std::unique_ptr<GameObject> cube_;
	std::unique_ptr<GameObject> player_;
};
