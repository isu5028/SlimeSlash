#include "DamageFloorComponent.h"

#include "ColoredPlaneComponent.h"
#include "GameTime.h"
#include "UnitHealthComponent.h"

#include <algorithm>

namespace
{
    constexpr float AppearDuration = 0.25f;
    constexpr float ActiveDuration = 10.0f;
    constexpr float DisappearDuration = 0.25f;
}

DamageFloorComponent::DamageFloorComponent(
    GameObject& owner,
    ColoredPlaneComponent& renderer,
    UnitHealthComponent& health)
    : Component(owner)
    , renderer_(renderer)
    , health_(health)
    , previousUpdate_(std::chrono::steady_clock::now())
{
    renderer_.SetVisible(true);
    renderer_.SetDissolveAmount(1.0f);
    health_.SetCanTakeDamage(false);
}

void DamageFloorComponent::BeginDisappearing()
{
    if (state_ == State::Disappearing || state_ == State::Hidden) return;

    state_ = State::Disappearing;
    stateElapsed_ = 0.0f;
    health_.SetCanTakeDamage(false);
}

void DamageFloorComponent::Update()
{
    const auto now = std::chrono::steady_clock::now();
    float deltaTime = std::chrono::duration<float>(now - previousUpdate_).count();
    previousUpdate_ = now;
    if (GameTime::IsHitStopped()) return;
    deltaTime = std::min(deltaTime, 0.05f);

    if (state_ == State::Appearing)
    {
        stateElapsed_ += deltaTime;
        const float progress = std::min(stateElapsed_ / AppearDuration, 1.0f);
        renderer_.SetDissolveAmount(1.0f - progress);
        if (progress >= 1.0f)
        {
            state_ = State::Active;
            stateElapsed_ = 0.0f;
            activeElapsed_ = 0.0f;
            renderer_.SetDissolveAmount(0.0f);
            health_.SetCanTakeDamage(true);
        }
        return;
    }

    if (state_ == State::Active)
    {
        // ActiveDuration経過後は見た目を消し始める
        // 実際のGameObject破棄はSceneHazardSystemがHiddenを検出したタイミングで行う
        activeElapsed_ += deltaTime;
        if (activeElapsed_ >= ActiveDuration)
        {
            BeginDisappearing();
        }
        return;
    }

    if (state_ == State::Disappearing)
    {
        stateElapsed_ += deltaTime;
        const float progress = std::min(stateElapsed_ / DisappearDuration, 1.0f);
        renderer_.SetDissolveAmount(progress);
        if (progress >= 1.0f)
        {
            renderer_.SetVisible(false);
            state_ = State::Hidden;
        }
    }
}

void DamageFloorComponent::ReceiveHit(
    const DirectX::XMFLOAT2& direction, float knockbackSpeed)
{
    BeginDisappearing();
}
