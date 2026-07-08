#pragma once

#include "Component.h"
#include "EnemyController.h"

#include <chrono>

class GameObject;
class PlayerControllerComponent;
class UnitVisualComponent;
class UnitHealthComponent;

class SlimeControllerComponent : public Component, public EnemyController
{
public:
    SlimeControllerComponent(
        GameObject& owner,
        GameObject& target,
        PlayerControllerComponent& targetController,
        UnitVisualComponent& visual,
        UnitHealthComponent& health);

    void Update() override;
    void ReceiveHit(const DirectX::XMFLOAT2& direction, float knockbackSpeed) override;
    void StartSpawn(int spawnIndex);
    bool IsDefeated() const { return state_ == State::Hidden; }

private:
    enum class State
    {
        Chasing,
        Resting,
        Dying,
        Escaping,
        Hidden,
        Spawning,
    };

    GameObject& target_;
    PlayerControllerComponent& targetController_;
    UnitVisualComponent& visual_;
    UnitHealthComponent& health_;
    void BeginEscape();
    State state_ = State::Chasing;
    float stateElapsed_ = 0.0f;
    float hitFlashTimeRemaining_ = 0.0f;
    float knockbackTimeRemaining_ = 0.0f;
    DirectX::XMFLOAT2 knockbackVelocity_ = { 0.0f, 0.0f };
    DirectX::XMFLOAT3 spawnStart_ = {};
    DirectX::XMFLOAT3 spawnEnd_ = {};
    DirectX::XMFLOAT3 escapeStart_ = {};
    DirectX::XMFLOAT3 escapeEnd_ = {};
    float spawnDelayRemaining_ = 0.0f;
    float deathDuration_ = 0.0f;
    std::chrono::steady_clock::time_point previousUpdate_;
};
