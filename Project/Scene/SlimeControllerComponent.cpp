#include "SlimeControllerComponent.h"

#include "GameObject.h"
#include "GameTime.h"
#include "PlayerControllerComponent.h"
#include "SoundPlayer.h"
#include "UnitVisualComponent.h"
#include "UnitHealthComponent.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr wchar_t SlimeExplodeSoundPath[] = L"Project/Assets/Sounds/SlimeExplode.wav";
    constexpr float ChaseDuration = 2.2f;
    constexpr float RestDuration = 1.0f;
    constexpr float MoveSpeed = 1.4f;
    constexpr float StopDistance = 0.75f;
    constexpr float GroundLimit = 4.5f;
    constexpr float HitFlashDuration = 0.10f;
    constexpr float KnockbackDuration = 0.20f;
    constexpr float SpawnInterval = 0.45f;
    constexpr float SpawnDuration = 1.0f;
    constexpr float SpawnArcHeight = 3.0f;
    constexpr float EscapeDuration = 0.95f;
    constexpr float EscapeArcHeight = 3.6f;
    constexpr float EscapeDistance = 6.5f;
    constexpr float GroundY = -1.25f;
    constexpr float GroundHalfSize = 5.0f;
    constexpr DirectX::XMFLOAT4 HitFlashColor = {
        0.45490196f, 0.27843139f, 0.23921569f, 1.0f
    };
}

SlimeControllerComponent::SlimeControllerComponent(
    GameObject& owner,
    GameObject& target,
    PlayerControllerComponent& targetController,
    UnitVisualComponent& visual,
    UnitHealthComponent& health)
    : Component(owner)
    , target_(target)
    , targetController_(targetController)
    , visual_(visual)
    , health_(health)
    , previousUpdate_(std::chrono::steady_clock::now())
{
}

void SlimeControllerComponent::BeginEscape()
{
    if (state_ == State::Escaping || state_ == State::Dying || state_ == State::Hidden)
        return;

    escapeStart_ = GetOwner().GetTransform().GetPosition();
    const auto& targetPosition = target_.GetTransform().GetPosition();
    float directionX = escapeStart_.x - targetPosition.x;
    float directionZ = escapeStart_.z - targetPosition.z;
    const float length = std::sqrt(directionX * directionX + directionZ * directionZ);
    if (length > 0.0001f)
    {
        directionX /= length;
        directionZ /= length;
    }
    else
    {
        directionX = escapeStart_.x >= 0.0f ? 1.0f : -1.0f;
        directionZ = 0.25f;
    }

    escapeEnd_ = {
        escapeStart_.x + directionX * EscapeDistance,
        GroundY,
        escapeStart_.z + directionZ * EscapeDistance
    };
    visual_.SetFacingFromHorizontalMovement(directionX);
    health_.SetCanTakeDamage(false);
    health_.SetBarVisible(false);
    visual_.SetHitboxVisible(false);
    knockbackTimeRemaining_ = 0.0f;
    hitFlashTimeRemaining_ = 0.0f;
    visual_.SetFlash(HitFlashColor, 0.0f);
    state_ = State::Escaping;
    stateElapsed_ = 0.0f;
}

void SlimeControllerComponent::StartSpawn(int spawnIndex)
{
    constexpr float laneOffsets[] = { 0.0f, -1.7f, 1.7f, -3.0f, 3.0f };
    const int side = spawnIndex % 4;
    const float laneOffset = laneOffsets[(spawnIndex / 4) % 5];
    if (side == 0)
    {
        spawnStart_ = { 6.2f, GroundY, laneOffset };
        spawnEnd_ = { 3.5f, GroundY, laneOffset * 0.7f };
    }
    else if (side == 1)
    {
        spawnStart_ = { laneOffset, GroundY, 6.2f };
        spawnEnd_ = { laneOffset * 0.7f, GroundY, 3.5f };
    }
    else if (side == 2)
    {
        spawnStart_ = { -6.2f, GroundY, laneOffset };
        spawnEnd_ = { -3.5f, GroundY, laneOffset * 0.7f };
    }
    else
    {
        spawnStart_ = { laneOffset, GroundY, -6.2f };
        spawnEnd_ = { laneOffset * 0.7f, GroundY, -3.5f };
    }
    spawnDelayRemaining_ = spawnIndex * SpawnInterval;
    health_.Reset();
    health_.SetCanTakeDamage(false);
    health_.SetBarVisible(false);
    GetOwner().GetTransform().SetPosition(spawnStart_);
    visual_.SetVisible(false);
    visual_.SetDissolveAmount(0.0f);
    visual_.SetFlash(HitFlashColor, 0.0f);
    visual_.SetHitboxVisible(false);
    visual_.SetShadowVisible(false);
    visual_.SetFacingFromHorizontalMovement(spawnEnd_.x - spawnStart_.x);
    visual_.PlayAnimation("Idle", true);
    state_ = State::Spawning;
    stateElapsed_ = 0.0f;
}

void SlimeControllerComponent::Update()
{
    const auto now = std::chrono::steady_clock::now();
    float deltaTime = std::chrono::duration<float>(now - previousUpdate_).count();
    previousUpdate_ = now;
    if (GameTime::IsHitStopped()) return;
    deltaTime = std::min(deltaTime, 0.05f);

    hitFlashTimeRemaining_ = std::max(0.0f, hitFlashTimeRemaining_ - deltaTime);
    visual_.SetFlash(HitFlashColor, hitFlashTimeRemaining_ > 0.0f ? 1.0f : 0.0f);

    if (targetController_.HasFullyDisappearedAfterDeath() &&
        state_ != State::Escaping &&
        state_ != State::Dying &&
        state_ != State::Hidden)
    {
        BeginEscape();
    }

    if (state_ == State::Dying)
    {
        stateElapsed_ += deltaTime;
        visual_.SetDissolveAmount(std::min(stateElapsed_ / deathDuration_, 1.0f));
        if (stateElapsed_ >= deathDuration_)
        {
            visual_.SetVisible(false);
            visual_.SetFlash(HitFlashColor, 0.0f);
            state_ = State::Hidden;
            stateElapsed_ = 0.0f;
        }
        return;
    }

    if (state_ == State::Escaping)
    {
        stateElapsed_ += deltaTime;
        const float t = std::min(stateElapsed_ / EscapeDuration, 1.0f);
        DirectX::XMFLOAT3 position = {
            escapeStart_.x + (escapeEnd_.x - escapeStart_.x) * t,
            GroundY + EscapeArcHeight * 4.0f * t * (1.0f - t),
            escapeStart_.z + (escapeEnd_.z - escapeStart_.z) * t
        };
        GetOwner().GetTransform().SetPosition(position);
        const bool overGround = std::abs(position.x) <= GroundHalfSize &&
            std::abs(position.z) <= GroundHalfSize;
        visual_.SetShadowVisible(overGround);
        visual_.SyncShadow();
        if (t >= 1.0f)
        {
            visual_.SetVisible(false);
            visual_.SetShadowVisible(false);
            state_ = State::Hidden;
            stateElapsed_ = 0.0f;
        }
        return;
    }

    if (state_ == State::Hidden)
    {
        return;
    }

    if (state_ == State::Spawning)
    {
        if (spawnDelayRemaining_ > 0.0f)
        {
            spawnDelayRemaining_ = std::max(
                0.0f, spawnDelayRemaining_ - deltaTime);
            return;
        }

        visual_.SetVisible(true);
        stateElapsed_ += deltaTime;
        const float t = std::min(stateElapsed_ / SpawnDuration, 1.0f);
        DirectX::XMFLOAT3 position = {
            spawnStart_.x + (spawnEnd_.x - spawnStart_.x) * t,
            GroundY + SpawnArcHeight * 4.0f * t * (1.0f - t),
            spawnStart_.z + (spawnEnd_.z - spawnStart_.z) * t
        };
        GetOwner().GetTransform().SetPosition(position);
        const bool overGround = std::abs(position.x) <= GroundHalfSize &&
            std::abs(position.z) <= GroundHalfSize;
        visual_.SetShadowVisible(overGround);
        visual_.SyncShadow();
        if (t >= 1.0f)
        {
            state_ = State::Chasing;
            stateElapsed_ = 0.0f;
            health_.SetCanTakeDamage(true);
            health_.SetBarVisible(true);
            visual_.SetHitboxVisible(true);
            visual_.SetShadowVisible(true);
        }
        return;
    }

    knockbackTimeRemaining_ = std::max(0.0f, knockbackTimeRemaining_ - deltaTime);

    stateElapsed_ += deltaTime;
    const float stateDuration = state_ == State::Chasing ? ChaseDuration : RestDuration;
    if (stateElapsed_ >= stateDuration)
    {
        stateElapsed_ -= stateDuration;
        state_ = state_ == State::Chasing ? State::Resting : State::Chasing;
    }

    auto position = GetOwner().GetTransform().GetPosition();
    if (knockbackTimeRemaining_ > 0.0f)
    {
        const float knockbackScale = knockbackTimeRemaining_ / KnockbackDuration;
        position.x += knockbackVelocity_.x * knockbackScale * deltaTime;
        position.z += knockbackVelocity_.y * knockbackScale * deltaTime;
    }
    else if (state_ == State::Chasing)
    {
        const auto& targetPosition = target_.GetTransform().GetPosition();
        const float offsetX = targetPosition.x - position.x;
        const float offsetZ = targetPosition.z - position.z;
        const float distance = std::sqrt(offsetX * offsetX + offsetZ * offsetZ);
        if (distance > StopDistance)
        {
            const float directionX = offsetX / distance;
            const float directionZ = offsetZ / distance;
            position.x += directionX * MoveSpeed * deltaTime;
            position.z += directionZ * MoveSpeed * deltaTime;
            visual_.SetFacingFromHorizontalMovement(directionX);
        }
    }

    position.x = std::max(-GroundLimit, std::min(position.x, GroundLimit));
    position.z = std::max(-GroundLimit, std::min(position.z, GroundLimit));
    GetOwner().GetTransform().SetPosition(position);
    visual_.SyncShadow();
}

void SlimeControllerComponent::ReceiveHit(
    const DirectX::XMFLOAT2& direction, float knockbackSpeed)
{
    if (state_ != State::Chasing && state_ != State::Resting) return;
    if (!health_.IsAlive())
    {
        state_ = State::Dying;
        stateElapsed_ = 0.0f;
        deathDuration_ = std::max(
            visual_.GetAnimationDurationSeconds("Die"), 0.1f);
        health_.SetCanTakeDamage(false);
        visual_.SetHitboxVisible(false);
        knockbackTimeRemaining_ = 0.0f;
        hitFlashTimeRemaining_ = HitFlashDuration;
        visual_.SetFlash(HitFlashColor, 1.0f);
        visual_.PlayAnimation("Die", true);
        SoundPlayer::Play(SlimeExplodeSoundPath);
        return;
    }

    const float length = std::sqrt(direction.x * direction.x + direction.y * direction.y);
    if (length <= 0.0001f) return;

    knockbackVelocity_.x = direction.x / length * knockbackSpeed;
    knockbackVelocity_.y = direction.y / length * knockbackSpeed;
    knockbackTimeRemaining_ = KnockbackDuration;
    hitFlashTimeRemaining_ = HitFlashDuration;
    visual_.SetFlash(HitFlashColor, 1.0f);
}
