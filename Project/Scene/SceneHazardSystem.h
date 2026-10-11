#pragma once

#include <memory>
#include <vector>

class ColoredPlaneComponent;
class DamageFloorComponent;
class EnemyTargetRegistry;
class GameObject;
class PlayerControllerComponent;
class Renderer;
class Slime5JumpShardComponent;
class UnitHealthComponent;
class UnitHitboxComponent;

class SceneHazardSystem
{
public:
    explicit SceneHazardSystem(EnemyTargetRegistry& targetRegistry);

    void Reset();
    void ClearQueuedSpawns();
    void QueueDamageFloorSpawn(const DirectX::XMFLOAT3& position, float delay);
    void SpawnDamageFloor(Renderer& renderer, const DirectX::XMFLOAT3& position);
    void SpawnSlime5JumpShards(
        Renderer& renderer,
        const DirectX::XMFLOAT3& position,
        float angle);
    void Update(Renderer& renderer, float deltaTime);
    void TryHitPlayer(
        PlayerControllerComponent& playerController,
        GameObject& player,
        const UnitHitboxComponent& playerHitbox) const;
    void ApplyFadeOut(float fadeAmount);
    void Draw(Renderer& renderer) const;
    void AppendUnitObjects(std::vector<GameObject*>& units) const;

private:
    // 敵が生成を依頼した危険物を、生成元の敵とは独立して管理する
    // そのため、敵が倒れてGameObjectごと破棄されても、床や破片は寿命まで残る
    struct QueuedDamageFloorSpawn
    {
        DirectX::XMFLOAT3 position = {};
        float delay = 0.0f;
    };

    struct DamageFloorInstance
    {
        // unique_ptrがGameObjectの所有権を持つ
        // vectorから削除されると、GameObjectと配下のComponentもまとめて破棄される
        std::unique_ptr<GameObject> object;
        ColoredPlaneComponent* renderer = nullptr;
        DamageFloorComponent* controller = nullptr;
        UnitHitboxComponent* hitbox = nullptr;
        UnitHealthComponent* health = nullptr;
        DirectX::XMFLOAT3 center = {};
        float halfWidth = 0.0f;
        float halfDepth = 0.0f;
        std::size_t targetIndex = 0;
    };

    struct Slime5JumpShardInstance
    {
        // 破片本体と影は別GameObject
        // 影は地面に固定し、本体だけを空中移動させる
        std::unique_ptr<GameObject> shadow;
        std::unique_ptr<GameObject> object;
        Slime5JumpShardComponent* controller = nullptr;
        UnitHitboxComponent* hitbox = nullptr;
        UnitHealthComponent* health = nullptr;
        std::size_t targetIndex = 0;
    };

    void SpawnDamageFloorTile(Renderer& renderer, const DirectX::XMFLOAT3& position);

    EnemyTargetRegistry& targetRegistry_;
    std::vector<DamageFloorInstance> damageFloors_;
    std::vector<Slime5JumpShardInstance> slime5JumpShards_;
    std::vector<QueuedDamageFloorSpawn> queuedDamageFloorSpawns_;
};
