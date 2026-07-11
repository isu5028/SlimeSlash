#include "SceneHazardSystem.h"

#include "AsepriteSpriteComponent.h"
#include "ColoredPlaneComponent.h"
#include "DamageFloorComponent.h"
#include "EnemyTargetRegistry.h"
#include "GameObject.h"
#include "PlayerControllerComponent.h"
#include "Renderer.h"
#include "Slime5JumpShardComponent.h"
#include "UnitHealthComponent.h"
#include "UnitHitboxComponent.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <random>

namespace
{
    constexpr float EnemyShadowWidth = 1.8f;
    constexpr float EnemyShadowDepth = 1.2f;
    constexpr float GroundHalfSize = 5.0f;
    constexpr float DamageFloorY = -0.98f;
    constexpr float DamageFloorTileWidth = EnemyShadowWidth * 0.5f;
    constexpr float DamageFloorTileDepth = EnemyShadowDepth * 0.5f;
    constexpr float DamageFloorHitboxHeight = 0.35f;
    constexpr float DamageFloorClusterPlacementLimitX =
        GroundHalfSize - DamageFloorTileWidth * 1.5f;
    constexpr float DamageFloorClusterPlacementLimitZ =
        GroundHalfSize - DamageFloorTileDepth * 1.5f;
    constexpr float DamageFloorTilePlacementLimitX =
        GroundHalfSize - DamageFloorTileWidth * 0.5f;
    constexpr float DamageFloorTilePlacementLimitZ =
        GroundHalfSize - DamageFloorTileDepth * 0.5f;
    constexpr float Slime5JumpShardShadowY = -0.99f;
    constexpr float Slime5JumpShardSpriteHeight = 0.7f;
    constexpr float Slime5JumpShardShadowSize = 0.34f;
    constexpr float Slime5JumpShardHitboxRadius = 0.24f;
    constexpr float Slime5JumpShardHitboxHeight = 0.42f;
    constexpr int Slime5JumpShardHealth = 1000000;
}

SceneHazardSystem::SceneHazardSystem(EnemyTargetRegistry& targetRegistry)
    : targetRegistry_(targetRegistry)
{
}

void SceneHazardSystem::Reset()
{
    for (const auto& floor : damageFloors_)
    {
        targetRegistry_.Release(floor.targetIndex);
    }
    for (const auto& shard : slime5JumpShards_)
    {
        targetRegistry_.Release(shard.targetIndex);
    }

    damageFloors_.clear();
    slime5JumpShards_.clear();
    queuedDamageFloorSpawns_.clear();
}

void SceneHazardSystem::ClearQueuedSpawns()
{
    queuedDamageFloorSpawns_.clear();
}

void SceneHazardSystem::QueueDamageFloorSpawn(
    const DirectX::XMFLOAT3& position,
    float delay)
{
    queuedDamageFloorSpawns_.push_back({ position, std::max(delay, 0.0f) });
}

void SceneHazardSystem::SpawnDamageFloor(
    Renderer& renderer,
    const DirectX::XMFLOAT3& position)
{
    const DirectX::XMFLOAT3 clampedPosition = {
        std::max(
            -DamageFloorClusterPlacementLimitX,
            std::min(position.x, DamageFloorClusterPlacementLimitX)),
        position.y,
        std::max(
            -DamageFloorClusterPlacementLimitZ,
            std::min(position.z, DamageFloorClusterPlacementLimitZ))
    };
    const float offsetX = DamageFloorTileWidth * 0.5f;
    const float offsetZ = DamageFloorTileDepth * 0.5f;
    const DirectX::XMFLOAT2 offsets[] = {
        { -offsetX, -offsetZ },
        {  offsetX, -offsetZ },
        { -offsetX,  offsetZ },
        {  offsetX,  offsetZ },
    };

    for (const auto& offset : offsets)
    {
        SpawnDamageFloorTile(renderer, {
            clampedPosition.x + offset.x,
            DamageFloorY,
            clampedPosition.z + offset.y
        });
    }
}

void SceneHazardSystem::SpawnDamageFloorTile(
    Renderer& renderer,
    const DirectX::XMFLOAT3& position)
{
    DamageFloorInstance floor;
    floor.object = std::make_unique<GameObject>();
    floor.center = {
        std::max(
            -DamageFloorTilePlacementLimitX,
            std::min(position.x, DamageFloorTilePlacementLimitX)),
        DamageFloorY,
        std::max(
            -DamageFloorTilePlacementLimitZ,
            std::min(position.z, DamageFloorTilePlacementLimitZ))
    };
    floor.object->GetTransform().SetPosition(floor.center);
    auto& floorRenderer = floor.object->AddComponent<ColoredPlaneComponent>();
    floorRenderer.Setup(
        renderer,
        DamageFloorTileWidth,
        DamageFloorTileDepth,
        { 0.0f, 0.0f, 0.0f, 1.0f });
    floorRenderer.SetDissolvePixelSize(0.0625f, 0.0625f);
    floorRenderer.SetUniformDissolve(true);
    auto& hitbox = floor.object->AddComponent<UnitHitboxComponent>();
    hitbox.Setup(
        renderer,
        std::min(DamageFloorTileWidth, DamageFloorTileDepth) * 0.42f,
        DamageFloorHitboxHeight,
        { 0.0f, 0.0f, 0.0f, 0.0f },
        false);
    auto& health = floor.object->AddComponent<UnitHealthComponent>(1);
    auto& controller = floor.object->AddComponent<DamageFloorComponent>(floorRenderer, health);
    floor.renderer = &floorRenderer;
    floor.controller = &controller;
    floor.hitbox = &hitbox;
    floor.health = &health;
    floor.halfWidth = DamageFloorTileWidth * 0.5f;
    floor.halfDepth = DamageFloorTileDepth * 0.5f;
    floor.targetIndex = targetRegistry_.Acquire({
        floor.object.get(), &hitbox, &hitbox, &health, &controller,
        false, true, { 0.0f, 0.0f, 0.0f, 1.0f }, 0.6666667f, 1
    });
    damageFloors_.push_back(std::move(floor));
}

void SceneHazardSystem::SpawnSlime5JumpShards(
    Renderer& renderer,
    const DirectX::XMFLOAT3& position,
    float angle)
{
    static std::mt19937 randomEngine(
        static_cast<std::mt19937::result_type>(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    std::uniform_real_distribution<float> jitterDistribution(-0.75f, 0.75f);
    const float angles[] = {
        angle + jitterDistribution(randomEngine),
        angle + DirectX::XM_PI + jitterDistribution(randomEngine),
        angle + DirectX::XM_PIDIV2 + jitterDistribution(randomEngine),
        angle - DirectX::XM_PIDIV2 + jitterDistribution(randomEngine),
    };
    const DirectX::XMFLOAT2 directions[] = {
        { std::cos(angles[0]), std::sin(angles[0]) },
        { std::cos(angles[1]), std::sin(angles[1]) },
        { std::cos(angles[2]), std::sin(angles[2]) },
        { std::cos(angles[3]), std::sin(angles[3]) },
    };

    for (const auto& direction : directions)
    {
        Slime5JumpShardInstance shard;
        shard.shadow = std::make_unique<GameObject>();
        shard.shadow->GetTransform().SetPosition({
            position.x, Slime5JumpShardShadowY, position.z
        });
        auto& shadowRenderer = shard.shadow->AddComponent<ColoredPlaneComponent>();
        shadowRenderer.Setup(
            renderer,
            Slime5JumpShardShadowSize,
            Slime5JumpShardShadowSize,
            { 0.05f, 0.05f, 0.05f, 0.50f });
        shadowRenderer.SetDissolvePixelSize(0.0625f, 0.0625f);

        shard.object = std::make_unique<GameObject>();
        shard.object->GetTransform().SetPosition(position);
        auto& sprite = shard.object->AddComponent<AsepriteSpriteComponent>();
        if (sprite.Setup(
            renderer,
            "Assets/Images/aseprite/Square.aseprite",
            Slime5JumpShardSpriteHeight))
        {
            sprite.Play("Idle", true);
        }

        auto& hitbox = shard.object->AddComponent<UnitHitboxComponent>();
        hitbox.Setup(
            renderer,
            Slime5JumpShardHitboxRadius,
            Slime5JumpShardHitboxHeight,
            { 0.0f, 0.0f, 0.0f, 0.0f },
            false);
        auto& health = shard.object->AddComponent<UnitHealthComponent>(Slime5JumpShardHealth);
        health.SetBarVisible(false);
        auto& controller = shard.object->AddComponent<Slime5JumpShardComponent>(
            *shard.shadow, shadowRenderer, sprite, health, direction);

        shard.controller = &controller;
        shard.hitbox = &hitbox;
        shard.health = &health;
        shard.targetIndex = targetRegistry_.Acquire({
            shard.object.get(), &hitbox, &hitbox, &health, &controller,
            true, false, { 1.0f, 1.0f, 1.0f, 1.0f }, 1.0f, -1
        });
        slime5JumpShards_.push_back(std::move(shard));
    }
}

void SceneHazardSystem::Update(Renderer& renderer, float deltaTime)
{
    for (auto& queuedSpawn : queuedDamageFloorSpawns_)
    {
        queuedSpawn.delay -= deltaTime;
    }
    for (size_t index = 0; index < queuedDamageFloorSpawns_.size();)
    {
        if (queuedDamageFloorSpawns_[index].delay > 0.0f)
        {
            ++index;
            continue;
        }

        SpawnDamageFloor(renderer, queuedDamageFloorSpawns_[index].position);
        queuedDamageFloorSpawns_.erase(queuedDamageFloorSpawns_.begin() + index);
    }

    for (auto& floor : damageFloors_)
    {
        if (floor.object && floor.controller)
        {
            floor.object->Update();
        }
    }

    damageFloors_.erase(std::remove_if(
        damageFloors_.begin(),
        damageFloors_.end(),
        [this](const DamageFloorInstance& floor)
        {
            if (!floor.controller || !floor.controller->IsExpired())
            {
                return false;
            }

            targetRegistry_.Release(floor.targetIndex);
            return true;
        }),
        damageFloors_.end());

    for (auto& shard : slime5JumpShards_)
    {
        if (shard.shadow)
        {
            shard.shadow->Update();
        }
        if (shard.object)
        {
            shard.object->Update();
        }
    }

    slime5JumpShards_.erase(std::remove_if(
        slime5JumpShards_.begin(),
        slime5JumpShards_.end(),
        [this](const Slime5JumpShardInstance& shard)
        {
            if (!shard.controller || !shard.controller->IsExpired())
            {
                return false;
            }

            targetRegistry_.Release(shard.targetIndex);
            return true;
        }),
        slime5JumpShards_.end());
}

void SceneHazardSystem::TryHitPlayer(
    PlayerControllerComponent& playerController,
    GameObject& player,
    const UnitHitboxComponent& playerHitbox) const
{
    const auto& playerPosition = player.GetTransform().GetPosition();
    if (playerPosition.y > DamageFloorY + 0.08f)
    {
        return;
    }

    for (const auto& floor : damageFloors_)
    {
        if (!floor.object || !floor.controller || !floor.controller->CanDamagePlayer())
        {
            continue;
        }

        const float offsetX = playerPosition.x - floor.center.x;
        const float offsetZ = playerPosition.z - floor.center.z;
        if (std::abs(offsetX) > floor.halfWidth + playerHitbox.GetRadius())
        {
            continue;
        }
        if (std::abs(offsetZ) > floor.halfDepth + playerHitbox.GetDepthRadius())
        {
            continue;
        }

        float knockbackX = offsetX;
        float knockbackZ = offsetZ;
        const float length = std::sqrt(knockbackX * knockbackX + knockbackZ * knockbackZ);
        if (length > 0.0001f)
        {
            knockbackX /= length;
            knockbackZ /= length;
        }
        else
        {
            knockbackX = 0.0f;
            knockbackZ = 1.0f;
        }

        auto impactPosition = playerPosition;
        impactPosition.y += playerHitbox.GetHeight() * 0.55f;
        if (playerController.ReceiveHazardDamage(
            { knockbackX, knockbackZ }, 2, impactPosition))
        {
            break;
        }
    }
}

void SceneHazardSystem::ApplyFadeOut(float fadeAmount)
{
    for (auto& floor : damageFloors_)
    {
        if (!floor.renderer)
        {
            continue;
        }

        floor.renderer->SetDissolveAmount(fadeAmount);
        floor.renderer->SetOpacity(1.0f - fadeAmount);
    }
}

void SceneHazardSystem::Draw(Renderer& renderer) const
{
    for (const auto& floor : damageFloors_)
    {
        if (floor.object)
        {
            floor.object->Draw(renderer);
        }
    }

    for (const auto& shard : slime5JumpShards_)
    {
        if (shard.shadow)
        {
            shard.shadow->Draw(renderer);
        }
    }
}

void SceneHazardSystem::AppendUnitObjects(std::vector<GameObject*>& units) const
{
    for (const auto& shard : slime5JumpShards_)
    {
        units.push_back(shard.object.get());
    }
}
