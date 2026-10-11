#pragma once

#include "Component.h"
#include "HitReceiver.h"

#include <chrono>

class ColoredPlaneComponent;
class UnitHealthComponent;

class DamageFloorComponent : public Component, public HitReceiver
{
public:
    DamageFloorComponent(
        GameObject& owner,
        ColoredPlaneComponent& renderer,
        UnitHealthComponent& health);

    void Update() override;
    void ReceiveHit(const DirectX::XMFLOAT2& direction, float knockbackSpeed) override;

    bool CanDamagePlayer() const { return state_ == State::Active; }
    bool IsExpired() const { return state_ == State::Hidden; }
    void BeginDisappearing();

private:
    enum class State
    {
        // 有効時間中でプレイヤーへのダメージ判定を行う
        Active,
        // 出現演出中でまだプレイヤーへダメージを与えない
        Appearing,
        // 消滅演出中でダメージ判定は停止している
        Disappearing,
        // 消滅済みで管理システムから削除される
        Hidden,
    };

    ColoredPlaneComponent& renderer_;
    UnitHealthComponent& health_;
    State state_ = State::Appearing;
    float stateElapsed_ = 0.0f;
    float activeElapsed_ = 0.0f;
    std::chrono::steady_clock::time_point previousUpdate_;
};
