#include "Slime5JumpShardComponent.h"

#include "AsepriteSpriteComponent.h"
#include "ColoredPlaneComponent.h"
#include "GameObject.h"
#include "GameTime.h"
#include "UnitHealthComponent.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr float GroundY = -1.25f;
    constexpr float ShadowY = -0.99f;
    constexpr float GroundLimitX = 4.8f;
    constexpr float GroundLimitZ = 4.8f;
    constexpr float LaunchHorizontalSpeed = 2.0f;
    constexpr float LaunchVerticalSpeed = 7.6f;
    constexpr float FallingGravity = 10.3f;
    constexpr float GroundedWaitDuration = 0.5f;
    constexpr float DissolveDuration = 0.22f;
    constexpr float KnockbackDuration = 0.18f;
    constexpr float SpinRate = DirectX::XM_PI * 5.0f;
}

Slime5JumpShardComponent::Slime5JumpShardComponent(
    GameObject& owner,
    GameObject& shadow,
    ColoredPlaneComponent& shadowRenderer,
    AsepriteSpriteComponent& sprite,
    UnitHealthComponent& health,
    const DirectX::XMFLOAT2& launchDirection)
    : Component(owner)
    , shadow_(shadow)
    , shadowRenderer_(shadowRenderer)
    , sprite_(sprite)
    , health_(health)
    , previousUpdate_(std::chrono::steady_clock::now())
{
    float directionX = launchDirection.x;
    float directionZ = launchDirection.y;
    const float length = std::sqrt(directionX * directionX + directionZ * directionZ);
    if (length > 0.0001f)
    {
        directionX /= length;
        directionZ /= length;
    }
    else
    {
        directionX = 1.0f;
        directionZ = 0.0f;
    }

    horizontalVelocity_ = {
        directionX * LaunchHorizontalSpeed,
        directionZ * LaunchHorizontalSpeed
    };
    verticalVelocity_ = LaunchVerticalSpeed;
    spinVelocity_ = (directionX >= 0.0f ? 1.0f : -1.0f) * SpinRate;

    sprite_.SetVisible(true);
    sprite_.SetDissolveAmount(0.0f);
    shadowRenderer_.SetVisible(true);
    shadowRenderer_.SetDissolveAmount(0.0f);
    health_.SetCanTakeDamage(true);
    SyncShadow();
}

void Slime5JumpShardComponent::SyncShadow()
{
    const auto position = GetOwner().GetTransform().GetPosition();
    shadow_.GetTransform().SetPosition({ position.x, ShadowY, position.z });
}

void Slime5JumpShardComponent::Update()
{
    const auto now = std::chrono::steady_clock::now();
    float deltaTime = std::chrono::duration<float>(now - previousUpdate_).count();
    previousUpdate_ = now;
    if (GameTime::IsHitStopped()) return;
    deltaTime = std::min(deltaTime, 0.05f);

    auto position = GetOwner().GetTransform().GetPosition();
    auto rotation = GetOwner().GetTransform().GetRotation();

    if (state_ == State::Flying)
    {
        knockbackTimeRemaining_ = std::max(0.0f, knockbackTimeRemaining_ - deltaTime);

        position.x += horizontalVelocity_.x * deltaTime;
        position.z += horizontalVelocity_.y * deltaTime;
        if (knockbackTimeRemaining_ > 0.0f)
        {
            const float knockbackScale = knockbackTimeRemaining_ / KnockbackDuration;
            position.x += knockbackVelocity_.x * knockbackScale * deltaTime;
            position.z += knockbackVelocity_.y * knockbackScale * deltaTime;
        }

        verticalVelocity_ -= FallingGravity * deltaTime;
        position.y += verticalVelocity_ * deltaTime;
        rotation.z += spinVelocity_ * deltaTime;

        position.x = std::max(-GroundLimitX, std::min(position.x, GroundLimitX));
        position.z = std::max(-GroundLimitZ, std::min(position.z, GroundLimitZ));

        if (position.y <= GroundY)
        {
            position.y = GroundY;
            state_ = State::Landed;
            stateElapsed_ = 0.0f;
            horizontalVelocity_ = { 0.0f, 0.0f };
            knockbackVelocity_ = { 0.0f, 0.0f };
            knockbackTimeRemaining_ = 0.0f;
            verticalVelocity_ = 0.0f;
            spinVelocity_ = 0.0f;
        }

        GetOwner().GetTransform().SetPosition(position);
        GetOwner().GetTransform().SetRotation(rotation);
        SyncShadow();
        return;
    }

    if (state_ == State::Landed)
    {
        stateElapsed_ += deltaTime;
        position.y = GroundY;
        GetOwner().GetTransform().SetPosition(position);
        GetOwner().GetTransform().SetRotation(rotation);
        SyncShadow();
        if (stateElapsed_ >= GroundedWaitDuration)
        {
            state_ = State::Dissolving;
            stateElapsed_ = 0.0f;
            health_.SetCanTakeDamage(false);
        }
        return;
    }

    if (state_ == State::Dissolving)
    {
        stateElapsed_ += deltaTime;
        const float progress = std::min(stateElapsed_ / DissolveDuration, 1.0f);
        sprite_.SetDissolveAmount(progress);
        shadowRenderer_.SetDissolveAmount(progress);
        GetOwner().GetTransform().SetPosition(position);
        GetOwner().GetTransform().SetRotation(rotation);
        SyncShadow();
        if (progress >= 1.0f)
        {
            sprite_.SetVisible(false);
            shadowRenderer_.SetVisible(false);
            state_ = State::Hidden;
        }
    }
}

void Slime5JumpShardComponent::ReceiveHit(
    const DirectX::XMFLOAT2& direction, float knockbackSpeed)
{
    if (state_ != State::Flying) return;

    const float length = std::sqrt(direction.x * direction.x + direction.y * direction.y);
    if (length <= 0.0001f) return;

    knockbackVelocity_ = {
        direction.x / length * knockbackSpeed,
        direction.y / length * knockbackSpeed
    };
    knockbackTimeRemaining_ = KnockbackDuration;
}
