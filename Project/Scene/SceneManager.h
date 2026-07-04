#pragma once

#include <memory>
#include <vector>
#include "Camera.h"
#include "EnemyTarget.h"
#include "GameObject.h"

class SlimeControllerComponent;

class SceneManager
{
public:
	static constexpr bool ShowAttackHitboxes = false;
	static constexpr bool ShowUnitHitboxes = false;

	SceneManager();
	~SceneManager();

	void Initialize(Renderer& renderer);
	void Terminate();

	void Update();
	void Draw();
private:
	struct EnemyInstance
	{
		std::unique_ptr<GameObject> shadow;
		std::unique_ptr<GameObject> object;
		std::unique_ptr<GameObject> healthBar;
		SlimeControllerComponent* controller = nullptr;
	};

	void CreateWave(int enemyCount);
	void CreateEnemy(int spawnIndex);

	Renderer* pRenderer_ = nullptr;
	Camera camera_;
	
	std::unique_ptr<GameObject> cube_;
	std::unique_ptr<GameObject> ground_;
	std::unique_ptr<GameObject> shadow_;
	std::unique_ptr<GameObject> player_;
	std::unique_ptr<GameObject> attackEffect_;
	std::unique_ptr<GameObject> impactParticles_;
	std::unique_ptr<GameObject> playerHealthBar_;
	std::vector<EnemyInstance> enemies_;
	std::vector<EnemyTarget> enemyTargets_;
	int waveEnemyCount_ = 1;
};
