#pragma once

#include "Component.h"
#include "HitReceiver.h"

#include <chrono>

class GameObject;
class UnitVisualComponent;
class UnitHealthComponent;

class SlimeControllerComponent : public Component, public HitReceiver
{
public:
    SlimeControllerComponent(
        GameObject& owner,
        GameObject& target,
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
        Hidden,
        Spawning,
    };

    GameObject& target_;
    UnitVisualComponent& visual_;
    UnitHealthComponent& health_;
    State state_ = State::Chasing;
    float stateElapsed_ = 0.0f;
    float hitFlashTimeRemaining_ = 0.0f;
    float knockbackTimeRemaining_ = 0.0f;
    DirectX::XMFLOAT2 knockbackVelocity_ = { 0.0f, 0.0f };
    DirectX::XMFLOAT3 spawnStart_ = {};
    DirectX::XMFLOAT3 spawnEnd_ = {};
    float spawnDelayRemaining_ = 0.0f;
    float deathDuration_ = 0.0f;
    std::chrono::steady_clock::time_point previousUpdate_;
};
