#include "SceneManager.h"
#include "ModelComponent.h"
#include "RotateComponent.h"
#include "Renderer.h"
#include "AsepriteSpriteComponent.h"

SceneManager::SceneManager()
{

}

SceneManager::~SceneManager()
{

}

void SceneManager::Initialize(Renderer& renderer)
{
	pRenderer_ = &renderer;
	
	/*
	cube_ = std::make_unique<GameObject>();
	cube_->GetTransform().SetPosition(
		{ 0.0f, 0.0f, 0.0f }
	);
	auto& model =
		cube_->AddComponent<ModelComponent>();
	model.Setup(renderer, "Project/Assets/Cube.fbx");
	cube_->AddComponent<RotateComponent>(0.01f);
	*/

	player_ = std::make_unique<GameObject>();
	player_->GetTransform().SetPosition({ 0.0f, -1.0f, -1.0f });
	auto& sprite = player_->AddComponent<AsepriteSpriteComponent>();
	if (sprite.Setup(renderer, "Project/Assets/Images/aseprite/Player.aseprite", 2.0f))
	{
		sprite.Play("Idle", true);
	}

}

void SceneManager::Terminate()
{
	cube_.reset();
	player_.reset();
}

void SceneManager::Update()
{
	camera_.Update();
	if (cube_)
	{
		cube_->Update();
	}
	if (player_)
	{
		player_->Update();
	}
}

void SceneManager::Draw()
{
	auto viewMatrix = camera_.GetViewMatrix();
	pRenderer_->SetupViewTransform(viewMatrix);

	if (cube_)
	{
		cube_->Draw(*pRenderer_);
	}
	if (player_)
	{
		player_->Draw(*pRenderer_);
	}
}
