#include "Slime2ControllerComponent.h"

#include "ChargeParticleComponent.h"
#include "ColoredPlaneComponent.h"
#include "GameObject.h"
#include "GameTime.h"
#include "PlayerControllerComponent.h"
#include "SoundPlayer.h"
#include "UnitHealthComponent.h"
#include "UnitVisualComponent.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr wchar_t SlimeRushSoundPath[] = L"Project/Assets/Sounds/SlimeRush.wav";
    constexpr float ChargeDuration = 1.15f;
    constexpr float TelegraphPauseDuration = 0.50f;
    constexpr float InterruptedPauseDuration = 0.50f;
    constexpr float DashDistance = 4.4f;
    constexpr float DashDuration = 0.48f;
    constexpr float TelegraphWidth = 0.30666667f;
    constexpr float TelegraphHeight = -0.99f;
    constexpr float ShakeAmplitude = 0.06f;
    constexpr float ShakeFrequency = 48.0f;
    constexpr float AimTurnSpeed = 4.2f;
    constexpr float GroundLimit = 4.5f;
    constexpr float HitFlashDuration = 0.10f;
    constexpr float KnockbackDuration = 0.12f;
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
    constexpr DirectX::XMFLOAT4 ChargeParticleColor = {
        0.45490196f, 0.27843139f, 0.23921569f, 1.0f
    };

    float EaseOutExpo(float value)
    {
        if (value >= 1.0f) return 1.0f;
        return 1.0f - std::pow(2.0f, -10.0f * value);
    }

    float NormalizeAngle(float value)
    {
        while (value > DirectX::XM_PI) value -= DirectX::XM_2PI;
        while (value < -DirectX::XM_PI) value += DirectX::XM_2PI;
        return value;
    }
}

Slime2ControllerComponent::Slime2ControllerComponent(
    GameObject& owner,
    GameObject& target,
    PlayerControllerComponent& targetController,
    UnitVisualComponent& visual,
    UnitHealthComponent& health,
    ChargeParticleComponent& chargeParticles,
    GameObject& telegraphArea,
    ColoredPlaneComponent& telegraphRenderer)
    : Component(owner)
    , target_(target)
    , targetController_(targetController)
    , visual_(visual)
    , health_(health)
    , chargeParticles_(chargeParticles)
    , telegraphArea_(telegraphArea)
    , telegraphRenderer_(telegraphRenderer)
    , previousUpdate_(std::chrono::steady_clock::now())
{
    HideTelegraph();
}

DirectX::XMFLOAT2 Slime2ControllerComponent::GetDirectionAwayFromPlayer()
{
    const auto& position = GetOwner().GetTransform().GetPosition();
    const auto& targetPosition = target_.GetTransform().GetPosition();
    DirectX::XMFLOAT2 direction = {
        position.x - targetPosition.x,
        position.z - targetPosition.z
    };
    const float length = std::sqrt(direction.x * direction.x + direction.y * direction.y);
    if (length <= 0.0001f) return { -1.0f, 0.0f };
    return { direction.x / length, direction.y / length };
}

void Slime2ControllerComponent::EmitChargeBurst(int count, float speedScale, float sizeScale)
{
    auto position = GetOwner().GetTransform().GetPosition();
    position.y += 0.22f;
    chargeParticles_.EmitBurst(
        position,
        GetDirectionAwayFromPlayer(),
        count,
        speedScale,
        sizeScale,
        ChargeParticleColor);
}

void Slime2ControllerComponent::HideTelegraph()
{
    telegraphRenderer_.SetVisible(false);
}

void Slime2ControllerComponent::SnapAimToPlayer(const DirectX::XMFLOAT3& position)
{
    const auto& targetPosition = target_.GetTransform().GetPosition();
    const float directionX = targetPosition.x - position.x;
    const float directionZ = targetPosition.z - position.z;
    const float length = std::sqrt(directionX * directionX + directionZ * directionZ);
    if (length <= 0.0001f) return;

    dashDirection_ = { directionX / length, directionZ / length };
    aimAngle_ = std::atan2(dashDirection_.x, dashDirection_.y);
}

void Slime2ControllerComponent::UpdateTelegraph(
    const DirectX::XMFLOAT3& position,
    const DirectX::XMFLOAT2& direction,
    float lengthScale)
{
    const float clampedScale = std::max(0.0f, std::min(lengthScale, 1.0f));
    if (clampedScale <= 0.0f)
    {
        HideTelegraph();
        return;
    }

    const float length = DashDistance * clampedScale;
    const float yaw = std::atan2(direction.x, direction.y);
    telegraphArea_.GetTransform().SetPosition({
        position.x + direction.x * (length * 0.5f),
        TelegraphHeight,
        position.z + direction.y * (length * 0.5f)
    });
    telegraphArea_.GetTransform().SetRotation({ 0.0f, yaw, 0.0f });
    telegraphArea_.GetTransform().SetScale({ TelegraphWidth, 1.0f, length });
    telegraphRenderer_.SetVisible(true);
}

void Slime2ControllerComponent::BeginEscape()
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
    HideTelegraph();
    state_ = State::Escaping;
    stateElapsed_ = 0.0f;
}

void Slime2ControllerComponent::BeginDying()
{
    if (state_ == State::Dying || state_ == State::Hidden) return;

    state_ = State::Dying;
    stateElapsed_ = 0.0f;
    deathDuration_ = std::max(
        visual_.GetAnimationDurationSeconds("Die"), 0.1f);
    health_.SetCanTakeDamage(false);
    visual_.SetHitboxVisible(false);
    knockbackTimeRemaining_ = 0.0f;
    hitFlashTimeRemaining_ = HitFlashDuration;
    visual_.SetFlash(HitFlashColor, 1.0f);
    HideTelegraph();
    visual_.PlayAnimation("Die", true);
}

void Slime2ControllerComponent::StartSpawn(int spawnIndex)
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
    HideTelegraph();
    state_ = State::Spawning;
    stateElapsed_ = 0.0f;
    dashDuration_ = 0.0f;
    telegraphPauseTime_ = 0.0f;
    telegraphAnchorPosition_ = spawnStart_;
}

void Slime2ControllerComponent::Update()
{
    const auto now = std::chrono::steady_clock::now();
    float deltaTime = std::chrono::duration<float>(now - previousUpdate_).count();
    previousUpdate_ = now;
    if (GameTime::IsHitStopped()) return;
    deltaTime = std::min(deltaTime, 0.05f);

    hitFlashTimeRemaining_ = std::max(0.0f, hitFlashTimeRemaining_ - deltaTime);
    visual_.SetFlash(HitFlashColor, hitFlashTimeRemaining_ > 0.0f ? 1.0f : 0.0f);

    if (!health_.IsAlive() &&
        state_ != State::Dying &&
        state_ != State::Escaping &&
        state_ != State::Hidden &&
        state_ != State::Spawning)
    {
        BeginDying();
    }

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
            HideTelegraph();
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
            HideTelegraph();
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
            spawnDelayRemaining_ = std::max(0.0f, spawnDelayRemaining_ - deltaTime);
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
            SnapAimToPlayer(position);
            state_ = State::Charging;
            stateElapsed_ = 0.0f;
            dashDuration_ = 0.0f;
            telegraphPauseTime_ = 0.0f;
            telegraphAnchorPosition_ = position;
            health_.SetCanTakeDamage(true);
            health_.SetBarVisible(true);
            visual_.SetHitboxVisible(true);
            visual_.SetShadowVisible(true);
        }
        return;
    }

    knockbackTimeRemaining_ = std::max(0.0f, knockbackTimeRemaining_ - deltaTime);
    auto position = GetOwner().GetTransform().GetPosition();
    if (knockbackTimeRemaining_ > 0.0f)
    {
        const float knockbackScale = knockbackTimeRemaining_ / KnockbackDuration;
        position.x += knockbackVelocity_.x * knockbackScale * deltaTime;
        position.z += knockbackVelocity_.y * knockbackScale * deltaTime;
    }

    if (state_ == State::Charging)
    {
        stateElapsed_ += deltaTime;
        telegraphAnchorPosition_ = position;

        const auto& targetPosition = target_.GetTransform().GetPosition();
        const float directionX = targetPosition.x - position.x;
        const float directionZ = targetPosition.z - position.z;
        const float length = std::sqrt(directionX * directionX + directionZ * directionZ);
        if (length > 0.0001f)
        {
            const float targetAngle = std::atan2(directionX, directionZ);
            const float angleDelta = NormalizeAngle(targetAngle - aimAngle_);
            const float step = std::min(std::abs(angleDelta), AimTurnSpeed * deltaTime);
            aimAngle_ += (angleDelta < 0.0f ? -step : step);
            aimAngle_ = NormalizeAngle(aimAngle_);
            dashDirection_ = { std::sin(aimAngle_), std::cos(aimAngle_) };
        }
        if (std::abs(dashDirection_.x) > 0.0001f)
        {
            visual_.SetFacingFromHorizontalMovement(dashDirection_.x);
        }
        UpdateTelegraph(telegraphAnchorPosition_, dashDirection_, stateElapsed_ / ChargeDuration);

        if (stateElapsed_ >= ChargeDuration)
        {
            telegraphAnchorPosition_ = position;
            UpdateTelegraph(telegraphAnchorPosition_, dashDirection_, 1.0f);
            telegraphPauseTime_ = TelegraphPauseDuration;
            state_ = State::TelegraphPause;
            stateElapsed_ = 0.0f;
        }
    }
    else if (state_ == State::TelegraphPause)
    {
        stateElapsed_ += deltaTime;
        telegraphPauseTime_ = std::max(0.0f, telegraphPauseTime_ - deltaTime);
        position = telegraphAnchorPosition_;
        UpdateTelegraph(telegraphAnchorPosition_, dashDirection_, 1.0f);

        const DirectX::XMFLOAT2 shakeSide = { -dashDirection_.y, dashDirection_.x };
        const float shakeOffset = std::sin(stateElapsed_ * ShakeFrequency) * ShakeAmplitude;
        position.x += shakeSide.x * shakeOffset;
        position.z += shakeSide.y * shakeOffset;

        if (telegraphPauseTime_ <= 0.0f)
        {
            dashStart_ = telegraphAnchorPosition_;
            dashDuration_ = DashDuration;
            state_ = State::Dashing;
            stateElapsed_ = 0.0f;
            HideTelegraph();
            EmitChargeBurst(4, 1.0f, 1.0f);
            SoundPlayer::Play(SlimeRushSoundPath);
        }
    }
    else if (state_ == State::Dashing)
    {
        stateElapsed_ += deltaTime;
        HideTelegraph();
        const float t = std::min(stateElapsed_ / std::max(dashDuration_, 0.0001f), 1.0f);
        const float dashProgress = EaseOutExpo(t);
        position.x = dashStart_.x + dashDirection_.x * DashDistance * dashProgress;
        position.z = dashStart_.z + dashDirection_.y * DashDistance * dashProgress;
        if (t >= 1.0f)
        {
            SnapAimToPlayer(position);
            state_ = State::Charging;
            stateElapsed_ = 0.0f;
            dashDuration_ = 0.0f;
            telegraphPauseTime_ = 0.0f;
            telegraphAnchorPosition_ = position;
        }
    }
    else if (state_ == State::Interrupted)
    {
        stateElapsed_ += deltaTime;
        HideTelegraph();
        if (stateElapsed_ >= InterruptedPauseDuration)
        {
            SnapAimToPlayer(position);
            state_ = State::Charging;
            stateElapsed_ = 0.0f;
            telegraphAnchorPosition_ = position;
        }
    }

    position.x = std::max(-GroundLimit, std::min(position.x, GroundLimit));
    position.z = std::max(-GroundLimit, std::min(position.z, GroundLimit));
    GetOwner().GetTransform().SetPosition(position);
    visual_.SyncShadow();
}

void Slime2ControllerComponent::ReceiveHit(
    const DirectX::XMFLOAT2& direction, float knockbackSpeed)
{
    if (!health_.IsAlive())
    {
        BeginDying();
        return;
    }
    if (state_ != State::Charging &&
        state_ != State::TelegraphPause &&
        state_ != State::Dashing &&
        state_ != State::Interrupted) return;

    const float length = std::sqrt(direction.x * direction.x + direction.y * direction.y);
    if (length <= 0.0001f) return;

    if (state_ == State::Dashing)
    {
        state_ = State::Interrupted;
        stateElapsed_ = 0.0f;
        dashDuration_ = 0.0f;
        telegraphPauseTime_ = 0.0f;
        telegraphAnchorPosition_ = GetOwner().GetTransform().GetPosition();
        HideTelegraph();
    }

    knockbackVelocity_.x = direction.x / length * knockbackSpeed;
    knockbackVelocity_.y = direction.y / length * knockbackSpeed;
    knockbackTimeRemaining_ = KnockbackDuration;
    hitFlashTimeRemaining_ = HitFlashDuration;
    visual_.SetFlash(HitFlashColor, 1.0f);
}
