#pragma once

#include "Component.h"
#include "EnemyController.h"

#include <chrono>
#include <functional>

class Camera;
class GameObject;
class PlayerControllerComponent;
class UnitHealthComponent;
class UnitVisualComponent;

class Slime4ControllerComponent : public Component, public EnemyController
{
public:
    Slime4ControllerComponent(
        GameObject& owner,
        GameObject& target,
        PlayerControllerComponent& targetController,
        UnitVisualComponent& visual,
        UnitHealthComponent& health,
        std::function<void(const DirectX::XMFLOAT3&)> spawnDamageFloor);

    void Update() override;
    void ReceiveHit(const DirectX::XMFLOAT2& direction, float knockbackSpeed) override;
    void StartSpawn(int spawnIndex, int spawnLocationIndex = -1) override;
    bool IsDefeated() const override { return state_ == State::Hidden; }

private:
    enum class State
    {
        Chasing,
        Dying,
        Escaping,
        Hidden,
        Resting,
        Spawning,
    };

    void BeginDying();
    void BeginEscape();

    GameObject& target_;
    PlayerControllerComponent& targetController_;
    UnitVisualComponent& visual_;
    UnitHealthComponent& health_;
    std::function<void(const DirectX::XMFLOAT3&)> spawnDamageFloor_;
    State state_ = State::Chasing;
    float stateElapsed_ = 0.0f;
    float hitFlashTimeRemaining_ = 0.0f;
    float knockbackTimeRemaining_ = 0.0f;
    float damageFloorDropRemaining_ = 0.0f;
    DirectX::XMFLOAT2 knockbackVelocity_ = { 0.0f, 0.0f };
    DirectX::XMFLOAT3 spawnStart_ = {};
    DirectX::XMFLOAT3 spawnEnd_ = {};
    DirectX::XMFLOAT3 escapeStart_ = {};
    DirectX::XMFLOAT3 escapeEnd_ = {};
    float spawnDelayRemaining_ = 0.0f;
    float deathDuration_ = 0.0f;
    std::chrono::steady_clock::time_point previousUpdate_;
};
