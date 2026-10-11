#pragma once

#include "Component.h"
#include "HitReceiver.h"

#include <chrono>

class AsepriteSpriteComponent;
class ColoredPlaneComponent;
class GameObject;
class UnitHealthComponent;

class Slime5JumpShardComponent : public Component, public HitReceiver
{
public:
    Slime5JumpShardComponent(
        GameObject& owner,
        GameObject& shadow,
        ColoredPlaneComponent& shadowRenderer,
        AsepriteSpriteComponent& sprite,
        UnitHealthComponent& health,
        const DirectX::XMFLOAT2& launchDirection);

    void Update() override;
    void ReceiveHit(const DirectX::XMFLOAT2& direction, float knockbackSpeed) override;

    bool IsExpired() const { return state_ == State::Hidden; }

private:
    enum class State
    {
        Flying,
        Landed,
        Dissolving,
        Hidden,
    };

    void SyncShadow();

    GameObject& shadow_;
    ColoredPlaneComponent& shadowRenderer_;
    AsepriteSpriteComponent& sprite_;
    UnitHealthComponent& health_;
    State state_ = State::Flying;
    float stateElapsed_ = 0.0f;
    float verticalVelocity_ = 0.0f;
    float spinVelocity_ = 0.0f;
    float knockbackTimeRemaining_ = 0.0f;
    DirectX::XMFLOAT2 horizontalVelocity_ = { 0.0f, 0.0f };
    DirectX::XMFLOAT2 knockbackVelocity_ = { 0.0f, 0.0f };
    std::chrono::steady_clock::time_point previousUpdate_;
};
