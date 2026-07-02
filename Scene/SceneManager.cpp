#include "SceneManager.h"
#include "Renderer.h"

SceneManager::SceneManager()
{

}

SceneManager::~SceneManager()
{

}

void SceneManager::Initialize(Renderer& renderer)
{
	pRenderer_ = &renderer;
	// sampleTriangle_.CreateVertexBuffer(renderer);
	const char* MODEL_PATH = "Assets/Cube.fbx";
	sampleModel_.Setup(renderer, MODEL_PATH);
}

void SceneManager::Terminate()
{
	// sampleTriangle_.DestroyVertexBuffer();
	sampleModel_.Terminate();
}

void SceneManager::Update()
{
	camera_.Update();
}

void SceneManager::Draw()
{
	auto viewMatrix = camera_.GetViewMatrix();
	pRenderer_->SetupViewTransform(viewMatrix);

	// sampleTriangle_.Draw(*pRenderer_);
	sampleModel_.Draw(*pRenderer_);
}
