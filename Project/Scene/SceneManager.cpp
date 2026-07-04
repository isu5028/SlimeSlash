#include "SceneManager.h"
#include "RotateComponent.h"
#include "Renderer.h"
#include "AsepriteSpriteComponent.h"
#include "ColoredBoxComponent.h"
#include "ColoredPlaneComponent.h"
#include "ImpactParticleComponent.h"
#include "HealthBarComponent.h"
#include "PlayerControllerComponent.h"
#include "SlashAttackEffectComponent.h"
#include "SlimeControllerComponent.h"
#include "UnitVisualComponent.h"
#include "UnitHitboxComponent.h"
#include "UnitHealthComponent.h"

#include <algorithm>
#include <vector>

SceneManager::SceneManager()
{

}

SceneManager::~SceneManager()
{

}

void SceneManager::Initialize(Renderer& renderer)
{
	pRenderer_ = &renderer;
	camera_.SetEyePosition({ 0.0f, 8.0f, -20.0f });
	camera_.SetFocusPosition({ 0.0f, -0.3f, -1.0f });

	// キャラクターが立つXZ平面の地面。
	ground_ = std::make_unique<GameObject>();
	ground_->GetTransform().SetPosition({ 0.0f, -1.0f, 0.0f });
	ground_->AddComponent<ColoredBoxComponent>().Setup(
		renderer, 10.0f, 1.0f, 10.0f,
		{ 0.67843139f, 0.41960785f, 0.41568628f, 1.0f });

	// 地面より少し上に半透明の長方形を置き、足元の簡易影にする。
	shadow_ = std::make_unique<GameObject>();
	shadow_->GetTransform().SetPosition({ 0.0f, -0.99f, -1.0f });
	auto& playerShadowRenderer = shadow_->AddComponent<ColoredPlaneComponent>();
	playerShadowRenderer.Setup(
		renderer, 1.0f, 1.0f, { 0.05f, 0.05f, 0.05f, 0.50f });

	player_ = std::make_unique<GameObject>();
	// 元画像の下端に約4pxの透明余白があるため、その分だけ下げて見た目の足を地面へ合わせる。
	player_->GetTransform().SetPosition({ 0.0f, -1.25f, -1.0f });
	auto& sprite = player_->AddComponent<AsepriteSpriteComponent>();
	if (sprite.Setup(renderer, "Assets/Images/aseprite/Player.aseprite", 2.0f))
	{
		sprite.Play("Idle", true);
	}

	attackEffect_ = std::make_unique<GameObject>();
	auto& attackEffect = attackEffect_->AddComponent<SlashAttackEffectComponent>(*player_);
	attackEffect.Setup(renderer);
	attackEffect.SetHitboxDebugVisible(ShowAttackHitboxes);
	impactParticles_ = std::make_unique<GameObject>();
	auto& impactParticles = impactParticles_->AddComponent<ImpactParticleComponent>(camera_);
	impactParticles.Setup(renderer);

	auto& playerVisual = player_->AddComponent<UnitVisualComponent>(
		*shadow_, playerShadowRenderer, sprite, 0.8f, 0.6f);
	auto& playerHitbox = player_->AddComponent<UnitHitboxComponent>();
	playerHitbox.Setup(renderer, 0.42f, 1.55f, { 0.15f, 0.55f, 1.0f, 0.25f }, ShowUnitHitboxes);
	playerVisual.SetHitbox(playerHitbox);
	auto& playerHealth = player_->AddComponent<UnitHealthComponent>(10);

	player_->AddComponent<PlayerControllerComponent>(
		camera_, impactParticles, sprite, playerVisual, playerHitbox, playerHealth,
		attackEffect, enemyTargets_, -1.25f);

	playerHealthBar_ = std::make_unique<GameObject>();
	playerHealthBar_->AddComponent<HealthBarComponent>(*player_, playerHealth).Setup(
		renderer, "Assets/Images/aseprite/HealthBar.aseprite", 1.0f, 1.75f);
	playerVisual.SyncShadow();
	CreateWave(waveEnemyCount_);

}

void SceneManager::CreateWave(int enemyCount)
{
	if (static_cast<int>(enemies_.size()) < enemyCount)
	{
		enemies_.reserve(enemyCount);
		enemyTargets_.reserve(enemyCount);
		while (static_cast<int>(enemies_.size()) < enemyCount)
		{
			CreateEnemy(static_cast<int>(enemies_.size()));
		}
	}

	for (int index = 0; index < enemyCount; ++index)
	{
		enemies_[index].controller->StartSpawn(index);
	}
}

void SceneManager::CreateEnemy(int spawnIndex)
{
	EnemyInstance enemy;
	enemy.shadow = std::make_unique<GameObject>();
	enemy.shadow->GetTransform().SetPosition({ 0.0f, -0.99f, 0.0f });
	auto& shadowRenderer = enemy.shadow->AddComponent<ColoredPlaneComponent>();
	shadowRenderer.Setup(
		*pRenderer_, 1.8f, 1.2f, { 0.05f, 0.05f, 0.05f, 0.50f });

	enemy.object = std::make_unique<GameObject>();
	auto& sprite = enemy.object->AddComponent<AsepriteSpriteComponent>();
	if (sprite.Setup(*pRenderer_, "Assets/Images/aseprite/Slime.aseprite", 2.0f))
	{
		sprite.Play("Idle", true);
	}
	auto& visual = enemy.object->AddComponent<UnitVisualComponent>(
		*enemy.shadow, shadowRenderer, sprite, 0.75f, 0.55f);
	auto& hitbox = enemy.object->AddComponent<UnitHitboxComponent>();
	hitbox.Setup(*pRenderer_, 0.80f, 0.80f,
		{ 1.0f, 0.25f, 0.15f, 0.25f }, ShowUnitHitboxes);
	visual.SetHitbox(hitbox);
	auto& health = enemy.object->AddComponent<UnitHealthComponent>(20);
	auto& controller = enemy.object->AddComponent<SlimeControllerComponent>(
		*player_, visual, health);
	enemy.controller = &controller;

	enemy.healthBar = std::make_unique<GameObject>();
	enemy.healthBar->AddComponent<HealthBarComponent>(
		*enemy.object, health).Setup(
			*pRenderer_, "Assets/Images/aseprite/HealthBar.aseprite", 1.0f, 1.25f);

	enemyTargets_.push_back({ &*enemy.object, &hitbox, &health, &controller });
	enemies_.push_back(std::move(enemy));
}

void SceneManager::Terminate()
{
	cube_.reset();
	ground_.reset();
	attackEffect_.reset();
	impactParticles_.reset();
	playerHealthBar_.reset();
	enemyTargets_.clear();
	enemies_.clear();
	player_.reset();
	shadow_.reset();
}

void SceneManager::Update()
{
	camera_.Update();
	if (cube_)
	{
		cube_->Update();
	}
	if (ground_)
	{
		ground_->Update();
	}
	if (shadow_)
	{
		shadow_->Update();
	}
	if (player_)
	{
		player_->Update();
	}
	for (auto& enemy : enemies_)
	{
		enemy.shadow->Update();
		enemy.object->Update();
		enemy.healthBar->Update();
	}
	if (attackEffect_)
	{
		attackEffect_->Update();
	}
	if (impactParticles_)
	{
		impactParticles_->Update();
	}
	if (playerHealthBar_) playerHealthBar_->Update();

	const bool waveDefeated = !enemies_.empty() && std::all_of(
		enemies_.begin(), enemies_.end(), [](const EnemyInstance& enemy)
		{
			return enemy.controller && enemy.controller->IsDefeated();
		});
	if (waveDefeated)
	{
		++waveEnemyCount_;
		CreateWave(waveEnemyCount_);
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
	if (ground_)
	{
		ground_->Draw(*pRenderer_);
	}
	if (shadow_)
	{
		shadow_->Draw(*pRenderer_);
	}
	for (auto& enemy : enemies_)
	{
		enemy.shadow->Draw(*pRenderer_);
	}
	if (attackEffect_)
	{
		attackEffect_->Draw(*pRenderer_);
	}
	std::vector<GameObject*> units;
	units.reserve(enemies_.size() + 1);
	units.push_back(player_.get());
	for (auto& enemy : enemies_) units.push_back(enemy.object.get());
	std::sort(units.begin(), units.end(), [&viewMatrix](GameObject* left, GameObject* right)
	{
		if (!left) return false;
		if (!right) return true;

		const auto leftPosition = DirectX::XMLoadFloat3(&left->GetTransform().GetPosition());
		const auto rightPosition = DirectX::XMLoadFloat3(&right->GetTransform().GetPosition());
		const float leftDepth = DirectX::XMVectorGetZ(
			DirectX::XMVector3TransformCoord(leftPosition, viewMatrix));
		const float rightDepth = DirectX::XMVectorGetZ(
			DirectX::XMVector3TransformCoord(rightPosition, viewMatrix));
		return leftDepth > rightDepth;
	});

	for (auto* unit : units)
	{
		if (unit) unit->Draw(*pRenderer_);
	}
	if (impactParticles_)
	{
		impactParticles_->Draw(*pRenderer_);
	}
	if (playerHealthBar_) playerHealthBar_->Draw(*pRenderer_);
	for (auto& enemy : enemies_) enemy.healthBar->Draw(*pRenderer_);
}
