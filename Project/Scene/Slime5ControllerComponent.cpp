#include "Slime5ControllerComponent.h"

#include "ChargeParticleComponent.h"
#include "ColoredDiskComponent.h"
#include "ColoredPlaneComponent.h"
#include "GameObject.h"
#include "GameTime.h"
#include "PlayerControllerComponent.h"
#include "SoundPlayer.h"
#include "UnitHealthComponent.h"
#include "UnitVisualComponent.h"

#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_set>
#include <utility>

namespace
{
    constexpr wchar_t SlimeRushSoundPath[] = L"Project/Assets/Sounds/SlimeRush.wav";
    constexpr wchar_t SlimeJumpSoundPath[] = L"Project/Assets/Sounds/SlimeJump.wav";
    constexpr wchar_t SlimeStampSoundPath[] = L"Project/Assets/Sounds/SlimeStamp.wav";
    constexpr wchar_t SlimeExplodeSoundPath[] = L"Project/Assets/Sounds/SlimeExplode.wav";
    constexpr wchar_t SlimeFloorSoundPath[] = L"Project/Assets/Sounds/SlimeFloor.wav";
    constexpr float ChaseDurationMin = 0.2f;
    constexpr float ChaseDurationMax = 0.4f;
    constexpr float ChaseMoveSpeed = 2.5f;
    constexpr float StrafeDurationMin = 1.0f;
    constexpr float StrafeDurationMax = 1.8f;
    constexpr float StrafeMoveSpeed = 1.7f;
    constexpr float DashWindupDuration = 0.30f;
    constexpr float TelegraphPauseDuration = 0.50f;
    constexpr float DashDuration = 0.34f;
    constexpr float DashDistance = 4.9f;
    constexpr float TelegraphWidth = 0.61333334f;
    constexpr float TelegraphHeight = -0.99f;
    constexpr float TelegraphAnchorOffsetZ = 0.6f;
    constexpr float RadialTelegraphDuration = 1.00f;
    constexpr float RadialShakeDuration = 0.30f;
    constexpr float RadialTelegraphRadius = 3.3f;
    constexpr float RadialFloorSpacingX = 1.35f;
    constexpr float RadialFloorSpacingZ = 0.92f;
    constexpr float RadialSpawnDelayStep = 0.045f;
    constexpr float RadialFloorCenterLimitX = 3.65f;
    constexpr float RadialFloorCenterLimitZ = 4.10f;
    constexpr float PlayerMaximumJumpVelocity = 14.65f;
    constexpr float JumpRiseGravity = 48.0f;
    constexpr float JumpDropGravity = 176.0f;
    constexpr float JumpHoverDuration = 0.25f;
    constexpr float JumpRecoverDuration = 0.25f;
    constexpr float JumpPatternDuration = 1.2f;
    constexpr float JumpHorizontalTravelScale = 1.0f;
    constexpr float StopDistance = 1.1f;
    constexpr float GroundLimitX = 3.6f;
    constexpr float GroundLimitZ = 4.3f;
    constexpr float HitFlashDuration = 0.10f;
    constexpr float KnockbackDuration = 0.24f;
    constexpr float ShakeAmplitude = 0.12f;
    constexpr float ShakeFrequency = 48.0f;
    constexpr float DeathHitStopDuration = 0.20f;
    constexpr float DeathEffectInterval = 0.225f;
    constexpr int DeathEffectCount = 8;
    constexpr float DeathEffectSequencePadding = 0.08f;
    constexpr float DeathEffectOffsetX = 0.85f;
    constexpr float DeathEffectOffsetYMin = 0.35f;
    constexpr float DeathEffectOffsetYMax = 1.45f;
    constexpr float DeathEffectOffsetZ = 0.25f;
    constexpr float DeathShakeAmplitudeX = 0.11f;
    constexpr float DeathShakeAmplitudeZ = 0.07f;
    constexpr float DeathShakeFrequencyX = 70.0f;
    constexpr float DeathShakeFrequencyZ = 53.0f;
    constexpr float DeathShakeDecay = 6.5f;
    constexpr float SpawnInterval = 0.45f;
    constexpr float SpawnDuration = 1.1f;
    constexpr float SpawnArcHeight = 3.8f;
    constexpr float EscapeDuration = 1.05f;
    constexpr float EscapeArcHeight = 4.2f;
    constexpr float EscapeDistance = 7.3f;
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

    DirectX::XMFLOAT3 ClampRadialFloorCenter(const DirectX::XMFLOAT3& position)
    {
        return {
            std::max(-RadialFloorCenterLimitX, std::min(position.x, RadialFloorCenterLimitX)),
            position.y,
            std::max(-RadialFloorCenterLimitZ, std::min(position.z, RadialFloorCenterLimitZ))
        };
    }

    std::int64_t MakeFloorCenterKey(const DirectX::XMFLOAT3& position)
    {
        const auto quantizedX = static_cast<std::int32_t>(std::round(position.x * 100.0f));
        const auto quantizedZ = static_cast<std::int32_t>(std::round(position.z * 100.0f));
        return (static_cast<std::int64_t>(quantizedX) << 32) |
            static_cast<std::uint32_t>(quantizedZ);
    }
}

Slime5ControllerComponent::Slime5ControllerComponent(
    GameObject& owner,
    GameObject& target,
    PlayerControllerComponent& targetController,
    UnitVisualComponent& visual,
    UnitHealthComponent& health,
    ChargeParticleComponent& chargeParticles,
    GameObject& telegraphArea,
    ColoredPlaneComponent& telegraphRenderer,
    GameObject& radialTelegraphArea,
    ColoredDiskComponent& radialTelegraphRenderer,
    std::function<void(const DirectX::XMFLOAT3&, float)> queueDamageFloorSpawn,
    std::function<void(const DirectX::XMFLOAT3&, float)> spawnJumpShards,
    std::function<void(const DirectX::XMFLOAT3&)> spawnDeathHitEffect,
    std::function<void()> triggerJumpImpactShake)
    : Component(owner)
    , target_(target)
    , targetController_(targetController)
    , visual_(visual)
    , health_(health)
    , chargeParticles_(chargeParticles)
    , telegraphArea_(telegraphArea)
    , telegraphRenderer_(telegraphRenderer)
    , radialTelegraphArea_(radialTelegraphArea)
    , radialTelegraphRenderer_(radialTelegraphRenderer)
    , queueDamageFloorSpawn_(std::move(queueDamageFloorSpawn))
    , spawnJumpShards_(std::move(spawnJumpShards))
    , spawnDeathHitEffect_(std::move(spawnDeathHitEffect))
    , triggerJumpImpactShake_(std::move(triggerJumpImpactShake))
    , previousUpdate_(std::chrono::steady_clock::now())
{
    HideTelegraph();
    HideRadialTelegraph();
}

DirectX::XMFLOAT2 Slime5ControllerComponent::GetDirectionAwayFromPlayer()
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

const char* Slime5ControllerComponent::PatternToString(Pattern pattern)
{
    switch (pattern)
    {
    case Pattern::Chase: return "Chase";
    case Pattern::Dash: return "Dash";
    case Pattern::RadialFloor: return "RadialFloor";
    case Pattern::Strafe: return "Strafe";
    case Pattern::JumpSlam: return "JumpSlam";
    default: return "Unknown";
    }
}

void Slime5ControllerComponent::EmitChargeBurst(int count, float speedScale, float sizeScale)
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

void Slime5ControllerComponent::HideTelegraph()
{
    telegraphRenderer_.SetVisible(false);
}

void Slime5ControllerComponent::HideRadialTelegraph()
{
    radialTelegraphRenderer_.SetVisible(false);
}

void Slime5ControllerComponent::UpdateTelegraph(
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
        position.z + TelegraphAnchorOffsetZ + direction.y * (length * 0.5f)
    });
    telegraphArea_.GetTransform().SetRotation({ 0.0f, yaw, 0.0f });
    telegraphArea_.GetTransform().SetScale({ TelegraphWidth, 1.0f, length });
    telegraphRenderer_.SetVisible(true);
}

void Slime5ControllerComponent::UpdateRadialTelegraph(
    const DirectX::XMFLOAT3& position, float radiusScale)
{
    const float clampedScale = std::max(0.0f, std::min(radiusScale, 1.0f));
    if (clampedScale <= 0.0f)
    {
        HideRadialTelegraph();
        return;
    }

    radialTelegraphArea_.GetTransform().SetPosition({
        position.x, TelegraphHeight, position.z
    });
    radialTelegraphArea_.GetTransform().SetScale({
        RadialTelegraphRadius * clampedScale,
        1.0f,
        RadialTelegraphRadius * clampedScale
    });
    radialTelegraphRenderer_.SetVisible(true);
}

void Slime5ControllerComponent::QueueRadialDamageFloors(const DirectX::XMFLOAT3& position)
{
    if (!queueDamageFloorSpawn_) return;

    const auto radialCenter = position;
    std::vector<std::pair<float, DirectX::XMFLOAT3>> floorPositions;
    std::unordered_set<std::int64_t> queuedFloorKeys;
    const int xSteps = static_cast<int>(std::ceil(RadialTelegraphRadius / RadialFloorSpacingX));
    const int zSteps = static_cast<int>(std::ceil(RadialTelegraphRadius / RadialFloorSpacingZ));

    const auto clampedCenter = ClampRadialFloorCenter(radialCenter);
    floorPositions.push_back({ 0.0f, clampedCenter });
    queuedFloorKeys.insert(MakeFloorCenterKey(clampedCenter));

    for (int x = -xSteps; x <= xSteps; ++x)
    {
        for (int z = -zSteps; z <= zSteps; ++z)
        {
            const DirectX::XMFLOAT3 unclampedFloorPosition = {
                radialCenter.x + static_cast<float>(x) * RadialFloorSpacingX,
                radialCenter.y,
                radialCenter.z + static_cast<float>(z) * RadialFloorSpacingZ
            };
            const float offsetX = unclampedFloorPosition.x - radialCenter.x;
            const float offsetZ = unclampedFloorPosition.z - radialCenter.z;
            const float distance = std::sqrt(offsetX * offsetX + offsetZ * offsetZ);
            if (distance > RadialTelegraphRadius) continue;
            const auto clampedFloorPosition =
                ClampRadialFloorCenter(unclampedFloorPosition);
            const auto floorKey = MakeFloorCenterKey(clampedFloorPosition);
            if (queuedFloorKeys.find(floorKey) != queuedFloorKeys.end()) continue;
            queuedFloorKeys.insert(floorKey);
            floorPositions.push_back({ distance, clampedFloorPosition });
        }
    }

    std::sort(
        floorPositions.begin(), floorPositions.end(),
        [](const auto& left, const auto& right) { return left.first < right.first; });

    float previousDistance = -1.0f;
    float delay = 0.0f;
    for (const auto& floorPosition : floorPositions)
    {
        if (previousDistance >= 0.0f && floorPosition.first - previousDistance > 0.2f)
        {
            delay += RadialSpawnDelayStep;
        }
        queueDamageFloorSpawn_(floorPosition.second, delay);
        previousDistance = floorPosition.first;
    }
    radialRecoverRemaining_ = delay + 0.25f;
    currentPatternDuration_ =
        RadialTelegraphDuration + RadialShakeDuration + radialRecoverRemaining_;
}

void Slime5ControllerComponent::StartJumpSlam(const DirectX::XMFLOAT3& position)
{
    HideTelegraph();
    HideRadialTelegraph();
    SoundPlayer::Play(SlimeJumpSoundPath);
    jumpHoverRemaining_ = JumpHoverDuration;
    jumpRecoverRemaining_ = JumpRecoverDuration;
    jumpVerticalVelocity_ = PlayerMaximumJumpVelocity;
    jumpTravelElapsed_ = 0.0f;
    jumpStartedDescending_ = false;
    jumpImpactTriggered_ = false;
    randomState_ = randomState_ * 1664525u + 1013904223u;
    jumpShardAngle_ =
        (static_cast<float>((randomState_ >> 8) & 0xffffu) / 65535.0f) * DirectX::XM_2PI;
    currentPatternDuration_ = JumpPatternDuration;

    const auto clampedStart = ClampJumpDestination(position);
    jumpStartPosition_ = clampedStart;
    const auto& targetPosition = target_.GetTransform().GetPosition();
    jumpDestination_ = ClampJumpDestination({
        targetPosition.x,
        GroundY,
        targetPosition.z
    });

    const float riseDuration =
        PlayerMaximumJumpVelocity / std::max(JumpRiseGravity, 0.0001f);
    const float jumpHeight =
        (PlayerMaximumJumpVelocity * PlayerMaximumJumpVelocity) /
        (2.0f * std::max(JumpRiseGravity, 0.0001f));
    const float fallDuration =
        std::sqrt((2.0f * jumpHeight) / std::max(JumpDropGravity, 0.0001f));
    jumpTravelDuration_ = std::max(
        (riseDuration + JumpHoverDuration + fallDuration) * JumpHorizontalTravelScale,
        0.0001f);
    jumpHorizontalVelocity_ = { 0.0f, 0.0f };

    const float offsetX = jumpDestination_.x - clampedStart.x;
    if (std::abs(offsetX) > 0.0001f)
    {
        visual_.SetFacingFromHorizontalMovement(offsetX);
    }
}

DirectX::XMFLOAT3 Slime5ControllerComponent::ClampJumpDestination(
    const DirectX::XMFLOAT3& position) const
{
    return {
        std::max(-GroundLimitX, std::min(position.x, GroundLimitX)),
        GroundY,
        std::max(-GroundLimitZ, std::min(position.z, GroundLimitZ))
    };
}

void Slime5ControllerComponent::BeginDying()
{
    if (state_ == State::Dying || state_ == State::Hidden) return;

    state_ = State::Dying;
    stateElapsed_ = 0.0f;
    deathBasePosition_ = GetOwner().GetTransform().GetPosition();
    deathDuration_ = std::max(
        visual_.GetAnimationDurationSeconds("Die"), 0.1f);
    deathEffectTimer_ = 0.0f;
    deathShakeStrength_ = 0.0f;
    deathEffectsRemaining_ = DeathEffectCount;
    deathDuration_ += DeathEffectInterval *
        static_cast<float>(DeathEffectCount - 1) + DeathEffectSequencePadding;
    health_.SetCanTakeDamage(false);
    health_.SetBarVisible(true);
    visual_.SetHitboxVisible(false);
    knockbackTimeRemaining_ = 0.0f;
    dashWindupRemaining_ = 0.0f;
    dashPauseRemaining_ = 0.0f;
    dashDurationRemaining_ = 0.0f;
    radialTelegraphRemaining_ = 0.0f;
    radialShakeRemaining_ = 0.0f;
    radialRecoverRemaining_ = 0.0f;
    jumpHoverRemaining_ = 0.0f;
    jumpRecoverRemaining_ = 0.0f;
    jumpVerticalVelocity_ = 0.0f;
    jumpTravelElapsed_ = 0.0f;
    jumpTravelDuration_ = 0.0f;
    jumpStartedDescending_ = false;
    jumpImpactTriggered_ = false;
    jumpHorizontalVelocity_ = { 0.0f, 0.0f };
    hitFlashTimeRemaining_ = HitFlashDuration;
    visual_.SetFlash(HitFlashColor, 1.0f);
    HideTelegraph();
    HideRadialTelegraph();
    visual_.PlayAnimation("Die", true);
    GameTime::StartHitStop(DeathHitStopDuration);
}

void Slime5ControllerComponent::BeginEscape()
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
    dashPauseRemaining_ = 0.0f;
    radialTelegraphRemaining_ = 0.0f;
    radialShakeRemaining_ = 0.0f;
    radialRecoverRemaining_ = 0.0f;
    jumpHoverRemaining_ = 0.0f;
    jumpRecoverRemaining_ = 0.0f;
    jumpVerticalVelocity_ = 0.0f;
    jumpTravelElapsed_ = 0.0f;
    jumpTravelDuration_ = 0.0f;
    jumpStartedDescending_ = false;
    jumpImpactTriggered_ = false;
    jumpHorizontalVelocity_ = { 0.0f, 0.0f };
    deathEffectTimer_ = 0.0f;
    deathEffectsRemaining_ = 0;
    visual_.SetFlash(HitFlashColor, 0.0f);
    HideTelegraph();
    HideRadialTelegraph();
    state_ = State::Escaping;
    stateElapsed_ = 0.0f;
}

Slime5ControllerComponent::Pattern Slime5ControllerComponent::ChooseNextPattern()
{
    if (!radialFloorUnlocked_ && health_.GetHealthRatio() <= 0.6f)
    {
        radialFloorUnlocked_ = true;
        radialFloorGuaranteedPending_ = true;
    }

    if (radialFloorGuaranteedPending_)
    {
        previousPattern_ = Pattern::RadialFloor;
        hasPreviousPattern_ = true;
        radialFloorGuaranteedPending_ = false;
        radialFloorRecoveryPending_ = true;
        return Pattern::RadialFloor;
    }

    std::array<Pattern, 5> candidates = {
        Pattern::Chase,
        Pattern::Dash,
        Pattern::RadialFloor,
        Pattern::Strafe,
        Pattern::JumpSlam
    };
    std::size_t candidateCount = 0;
    for (const Pattern candidate : candidates)
    {
        if (candidate == Pattern::RadialFloor &&
            (!radialFloorUnlocked_ || radialFloorRecoveryPending_))
        {
            continue;
        }
        if (hasPreviousPattern_)
        {
            const bool previousPatternWasWalking =
                previousPattern_ == Pattern::Chase || previousPattern_ == Pattern::Strafe;
            const bool candidateIsWalking =
                candidate == Pattern::Chase || candidate == Pattern::Strafe;
            if (previousPatternWasWalking && candidateIsWalking)
            {
                continue;
            }
            const bool disallowConsecutiveChase =
                candidate == Pattern::Chase && previousPattern_ == Pattern::Chase;
            const bool disallowConsecutiveRadialFloor =
                candidate == Pattern::RadialFloor &&
                previousPattern_ == Pattern::RadialFloor;
            if (disallowConsecutiveChase || disallowConsecutiveRadialFloor)
            {
                continue;
            }
        }
        candidates[candidateCount++] = candidate;
    }

    if (candidateCount == 0)
    {
        candidates[0] = Pattern::Dash;
        candidateCount = 1;
    }

    randomState_ = randomState_ * 1664525u + 1013904223u;
    const Pattern next = candidates[(randomState_ >> 16) % candidateCount];
    if (next == Pattern::RadialFloor)
    {
        radialFloorRecoveryPending_ = true;
    }
    else if (next == Pattern::Dash || next == Pattern::JumpSlam)
    {
        radialFloorRecoveryPending_ = false;
    }
    previousPattern_ = next;
    hasPreviousPattern_ = true;
    return next;
}

void Slime5ControllerComponent::SnapDashDirectionToPlayer(const DirectX::XMFLOAT3& position)
{
    const auto& targetPosition = target_.GetTransform().GetPosition();
    const float offsetX = targetPosition.x - position.x;
    const float offsetZ = targetPosition.z - position.z;
    const float length = std::sqrt(offsetX * offsetX + offsetZ * offsetZ);
    if (length <= 0.0001f)
    {
        dashDirection_ = { 0.0f, 1.0f };
        return;
    }

    dashDirection_ = { offsetX / length, offsetZ / length };
}

void Slime5ControllerComponent::StartPattern(const DirectX::XMFLOAT3& position)
{
    activePattern_ = ChooseNextPattern();
    patternElapsed_ = 0.0f;
    dashWindupRemaining_ = 0.0f;
    dashPauseRemaining_ = 0.0f;
    dashDurationRemaining_ = 0.0f;
    radialTelegraphRemaining_ = 0.0f;
    radialShakeRemaining_ = 0.0f;
    radialRecoverRemaining_ = 0.0f;
    jumpHoverRemaining_ = 0.0f;
    jumpRecoverRemaining_ = 0.0f;
    jumpVerticalVelocity_ = 0.0f;
    jumpTravelElapsed_ = 0.0f;
    jumpTravelDuration_ = 0.0f;
    jumpStartedDescending_ = false;
    jumpImpactTriggered_ = false;
    jumpHorizontalVelocity_ = { 0.0f, 0.0f };
    telegraphAnchorPosition_ = spawnStart_;
    radialCenterPosition_ = position;

    if (activePattern_ == Pattern::Chase)
    {
        HideRadialTelegraph();
        randomState_ = randomState_ * 1664525u + 1013904223u;
        const float randomValue = static_cast<float>((randomState_ >> 8) & 0xffffu) / 65535.0f;
        currentPatternDuration_ =
            ChaseDurationMin + (ChaseDurationMax - ChaseDurationMin) * randomValue;
    }
    else if (activePattern_ == Pattern::Dash)
    {
        HideRadialTelegraph();
        currentPatternDuration_ =
            DashWindupDuration + TelegraphPauseDuration + DashDuration;
        dashWindupRemaining_ = DashWindupDuration;
        dashPauseRemaining_ = TelegraphPauseDuration;
        dashDurationRemaining_ = DashDuration;
        SnapDashDirectionToPlayer(position);
        telegraphAnchorPosition_ = position;
        visual_.SetFacingFromHorizontalMovement(dashDirection_.x);
        UpdateTelegraph(position, dashDirection_, 0.0f);
    }
    else if (activePattern_ == Pattern::RadialFloor)
    {
        currentPatternDuration_ = RadialTelegraphDuration + RadialShakeDuration;
        radialTelegraphRemaining_ = RadialTelegraphDuration;
        radialShakeRemaining_ = RadialShakeDuration;
        radialRecoverRemaining_ = 0.0f;
        radialCenterPosition_ = position;
        HideTelegraph();
        UpdateRadialTelegraph(radialCenterPosition_, 0.0f);
    }
    else if (activePattern_ == Pattern::JumpSlam)
    {
        StartJumpSlam(position);
    }
    else
    {
        HideTelegraph();
        HideRadialTelegraph();
        randomState_ = randomState_ * 1664525u + 1013904223u;
        const float randomValue = static_cast<float>((randomState_ >> 8) & 0xffffu) / 65535.0f;
        currentPatternDuration_ =
            StrafeDurationMin + (StrafeDurationMax - StrafeDurationMin) * randomValue;
        const auto& targetPosition = target_.GetTransform().GetPosition();
        const float offsetX = targetPosition.x - position.x;
        const float offsetZ = targetPosition.z - position.z;
        const float length = std::sqrt(offsetX * offsetX + offsetZ * offsetZ);
        DirectX::XMFLOAT2 toPlayer = { 0.0f, 1.0f };
        if (length > 0.0001f)
        {
            toPlayer = { offsetX / length, offsetZ / length };
        }
        const bool clockwise = (randomState_ & 1u) == 0u;
        strafeDirection_ = clockwise
            ? DirectX::XMFLOAT2{ toPlayer.y, -toPlayer.x }
            : DirectX::XMFLOAT2{ -toPlayer.y, toPlayer.x };
        visual_.SetFacingFromHorizontalMovement(strafeDirection_.x);
    }
}

void Slime5ControllerComponent::StartSpawn(int spawnIndex)
{
    constexpr float laneOffsets[] = { 0.0f, -1.7f, 1.7f, -3.0f, 3.0f };
    const int side = spawnIndex % 4;
    const float laneOffset = laneOffsets[(spawnIndex / 4) % 5];
    if (side == 0)
    {
        spawnStart_ = { 7.1f, GroundY, laneOffset };
        spawnEnd_ = { 3.4f, GroundY, laneOffset * 0.6f };
    }
    else if (side == 1)
    {
        spawnStart_ = { laneOffset, GroundY, 7.1f };
        spawnEnd_ = { laneOffset * 0.6f, GroundY, 3.4f };
    }
    else if (side == 2)
    {
        spawnStart_ = { -7.1f, GroundY, laneOffset };
        spawnEnd_ = { -3.4f, GroundY, laneOffset * 0.6f };
    }
    else
    {
        spawnStart_ = { laneOffset, GroundY, -7.1f };
        spawnEnd_ = { laneOffset * 0.6f, GroundY, -3.4f };
    }

    spawnDelayRemaining_ = spawnIndex * SpawnInterval;
    randomState_ ^= static_cast<std::uint32_t>(spawnIndex + 1) * 0x9e3779b9u;
    randomState_ ^= static_cast<std::uint32_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    hasPreviousPattern_ = false;
    radialFloorUnlocked_ = false;
    radialFloorGuaranteedPending_ = false;
    radialFloorRecoveryPending_ = false;
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
    HideRadialTelegraph();
    state_ = State::Spawning;
    stateElapsed_ = 0.0f;
    patternElapsed_ = 0.0f;
    dashWindupRemaining_ = 0.0f;
    dashPauseRemaining_ = 0.0f;
    dashDurationRemaining_ = 0.0f;
    radialTelegraphRemaining_ = 0.0f;
    radialShakeRemaining_ = 0.0f;
    radialRecoverRemaining_ = 0.0f;
    jumpHoverRemaining_ = 0.0f;
    jumpRecoverRemaining_ = 0.0f;
    jumpVerticalVelocity_ = 0.0f;
    jumpTravelElapsed_ = 0.0f;
    jumpTravelDuration_ = 0.0f;
    jumpStartedDescending_ = false;
    jumpImpactTriggered_ = false;
    jumpHorizontalVelocity_ = { 0.0f, 0.0f };
    deathEffectTimer_ = 0.0f;
    deathEffectsRemaining_ = 0;
}

void Slime5ControllerComponent::Update()
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
        while (deathEffectsRemaining_ > 0 && stateElapsed_ >= deathEffectTimer_)
        {
            if (spawnDeathHitEffect_)
            {
                auto position = GetOwner().GetTransform().GetPosition();
                randomState_ = randomState_ * 1664525u + 1013904223u;
                const float randomX =
                    (static_cast<float>((randomState_ >> 8) & 0xffffu) / 65535.0f) *
                    2.0f - 1.0f;
                randomState_ = randomState_ * 1664525u + 1013904223u;
                const float randomY =
                    static_cast<float>((randomState_ >> 8) & 0xffffu) / 65535.0f;
                randomState_ = randomState_ * 1664525u + 1013904223u;
                const float randomZ =
                    (static_cast<float>((randomState_ >> 8) & 0xffffu) / 65535.0f) *
                    2.0f - 1.0f;
                position.x += randomX * DeathEffectOffsetX;
                position.y += DeathEffectOffsetYMin +
                    (DeathEffectOffsetYMax - DeathEffectOffsetYMin) * randomY;
                position.z += randomZ * DeathEffectOffsetZ;
                spawnDeathHitEffect_(position);
                SoundPlayer::Play(SlimeExplodeSoundPath);
            }
            deathShakeStrength_ = 1.0f;
            --deathEffectsRemaining_;
            deathEffectTimer_ += DeathEffectInterval;
        }

        deathShakeStrength_ = std::max(
            0.0f, deathShakeStrength_ - deltaTime * DeathShakeDecay);
        GetOwner().GetTransform().SetPosition({
            deathBasePosition_.x +
                std::sin(stateElapsed_ * DeathShakeFrequencyX) *
                DeathShakeAmplitudeX * deathShakeStrength_,
            deathBasePosition_.y,
            deathBasePosition_.z +
                std::cos(stateElapsed_ * DeathShakeFrequencyZ) *
                DeathShakeAmplitudeZ * deathShakeStrength_
        });

        const float dissolveStartTime =
            DeathEffectInterval * static_cast<float>(DeathEffectCount - 1) +
            DeathEffectSequencePadding;
        const float dissolveProgress = std::max(0.0f, stateElapsed_ - dissolveStartTime) /
            std::max(deathDuration_ - dissolveStartTime, 0.0001f);
        visual_.SetDissolveAmount(std::min(dissolveProgress, 1.0f));
        if (stateElapsed_ >= deathDuration_)
        {
            visual_.SetVisible(false);
            visual_.SetFlash(HitFlashColor, 0.0f);
            HideTelegraph();
            HideRadialTelegraph();
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
            HideRadialTelegraph();
            state_ = State::Hidden;
            stateElapsed_ = 0.0f;
        }
        return;
    }

    if (state_ == State::Hidden) return;

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
            state_ = State::Acting;
            stateElapsed_ = 0.0f;
            health_.SetCanTakeDamage(true);
            health_.SetBarVisible(true);
            visual_.SetHitboxVisible(true);
            visual_.SetShadowVisible(true);
            StartPattern(position);
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

    patternElapsed_ += deltaTime;
    if (activePattern_ == Pattern::Chase && knockbackTimeRemaining_ <= 0.0f)
    {
        const auto& targetPosition = target_.GetTransform().GetPosition();
        const float offsetX = targetPosition.x - position.x;
        const float offsetZ = targetPosition.z - position.z;
        const float distance = std::sqrt(offsetX * offsetX + offsetZ * offsetZ);
        if (distance > StopDistance)
        {
            const float directionX = offsetX / distance;
            const float directionZ = offsetZ / distance;
            position.x += directionX * ChaseMoveSpeed * deltaTime;
            position.z += directionZ * ChaseMoveSpeed * deltaTime;
            visual_.SetFacingFromHorizontalMovement(directionX);
        }
    }
    else if (activePattern_ == Pattern::Strafe && knockbackTimeRemaining_ <= 0.0f)
    {
        const auto& targetPosition = target_.GetTransform().GetPosition();
        const float offsetX = targetPosition.x - position.x;
        const float offsetZ = targetPosition.z - position.z;
        const float distance = std::sqrt(offsetX * offsetX + offsetZ * offsetZ);
        if (distance > StopDistance)
        {
            const float pullScale = 0.55f / std::max(distance, 0.0001f);
            position.x += (strafeDirection_.x + offsetX * pullScale) * StrafeMoveSpeed * deltaTime;
            position.z += (strafeDirection_.y + offsetZ * pullScale) * StrafeMoveSpeed * deltaTime;
            visual_.SetFacingFromHorizontalMovement(strafeDirection_.x);
        }
    }
    else if (activePattern_ == Pattern::Dash)
    {
        HideRadialTelegraph();
        if (dashWindupRemaining_ > 0.0f)
        {
            dashWindupRemaining_ = std::max(0.0f, dashWindupRemaining_ - deltaTime);
            telegraphAnchorPosition_ = position;
            SnapDashDirectionToPlayer(position);
            visual_.SetFacingFromHorizontalMovement(dashDirection_.x);
            const float windupProgress =
                1.0f - dashWindupRemaining_ / std::max(DashWindupDuration, 0.0001f);
            UpdateTelegraph(telegraphAnchorPosition_, dashDirection_, windupProgress);
            if (dashWindupRemaining_ <= 0.0f)
            {
                telegraphAnchorPosition_ = position;
                UpdateTelegraph(telegraphAnchorPosition_, dashDirection_, 1.0f);
            }
        }
        else if (dashPauseRemaining_ > 0.0f)
        {
            dashPauseRemaining_ = std::max(0.0f, dashPauseRemaining_ - deltaTime);
            position = telegraphAnchorPosition_;
            UpdateTelegraph(telegraphAnchorPosition_, dashDirection_, 1.0f);

            const DirectX::XMFLOAT2 shakeSide = { -dashDirection_.y, dashDirection_.x };
            const float shakeOffset = std::sin(
                (TelegraphPauseDuration - dashPauseRemaining_) * ShakeFrequency) *
                ShakeAmplitude;
            position.x += shakeSide.x * shakeOffset;
            position.z += shakeSide.y * shakeOffset;

            if (dashPauseRemaining_ <= 0.0f)
            {
                HideTelegraph();
                EmitChargeBurst(4, 1.0f, 1.0f);
                SoundPlayer::Play(SlimeRushSoundPath);
            }
        }
        else if (dashDurationRemaining_ > 0.0f)
        {
            HideTelegraph();
            const float elapsedDashTime = DashDuration - dashDurationRemaining_;
            dashDurationRemaining_ = std::max(0.0f, dashDurationRemaining_ - deltaTime);
            const float previousT = std::min(elapsedDashTime / DashDuration, 1.0f);
            const float currentT =
                std::min((elapsedDashTime + deltaTime) / DashDuration, 1.0f);
            const float travel =
                (EaseOutExpo(currentT) - EaseOutExpo(previousT)) * DashDistance;
            position.x += dashDirection_.x * travel;
            position.z += dashDirection_.y * travel;
        }
    }
    else if (activePattern_ == Pattern::RadialFloor)
    {
        HideTelegraph();
        if (radialTelegraphRemaining_ > 0.0f)
        {
            radialTelegraphRemaining_ = std::max(0.0f, radialTelegraphRemaining_ - deltaTime);
            radialCenterPosition_ = position;
            const float progress =
                1.0f - radialTelegraphRemaining_ /
                std::max(RadialTelegraphDuration, 0.0001f);
            UpdateRadialTelegraph(radialCenterPosition_, progress);
        }
        else if (radialShakeRemaining_ > 0.0f)
        {
            radialShakeRemaining_ = std::max(0.0f, radialShakeRemaining_ - deltaTime);
            radialCenterPosition_ = position;
            UpdateRadialTelegraph(radialCenterPosition_, 1.0f);

            const auto& targetPosition = target_.GetTransform().GetPosition();
            float awayX = radialCenterPosition_.x - targetPosition.x;
            float awayZ = radialCenterPosition_.z - targetPosition.z;
            const float awayLength = std::sqrt(awayX * awayX + awayZ * awayZ);
            if (awayLength > 0.0001f)
            {
                awayX /= awayLength;
                awayZ /= awayLength;
            }
            else
            {
                awayX = 1.0f;
                awayZ = 0.0f;
            }

            const float shakeOffset = std::sin(
                (RadialShakeDuration - radialShakeRemaining_) * ShakeFrequency) *
                ShakeAmplitude;
            position.x += awayX * shakeOffset;
            position.z += awayZ * shakeOffset;

            if (radialShakeRemaining_ <= 0.0f)
            {
                SoundPlayer::Play(SlimeFloorSoundPath);
                QueueRadialDamageFloors(radialCenterPosition_);
                HideRadialTelegraph();
            }
        }
        else if (radialRecoverRemaining_ > 0.0f)
        {
            radialRecoverRemaining_ = std::max(0.0f, radialRecoverRemaining_ - deltaTime);
            HideRadialTelegraph();
        }
    }
    else if (activePattern_ == Pattern::JumpSlam)
    {
        HideTelegraph();
        HideRadialTelegraph();
        jumpTravelElapsed_ = std::min(jumpTravelElapsed_ + deltaTime, jumpTravelDuration_);
        const float travelProgress = std::min(
            jumpTravelElapsed_ / std::max(jumpTravelDuration_, 0.0001f), 1.0f);
        position.x = jumpStartPosition_.x +
            (jumpDestination_.x - jumpStartPosition_.x) * travelProgress;
        position.z = jumpStartPosition_.z +
            (jumpDestination_.z - jumpStartPosition_.z) * travelProgress;

        if (!jumpStartedDescending_)
        {
            if (jumpVerticalVelocity_ > 0.0f)
            {
                jumpVerticalVelocity_ -= JumpRiseGravity * deltaTime;
                position.y += jumpVerticalVelocity_ * deltaTime;
                if (jumpVerticalVelocity_ <= 0.0f)
                {
                    jumpVerticalVelocity_ = 0.0f;
                }
            }
            else if (jumpHoverRemaining_ > 0.0f)
            {
                jumpHoverRemaining_ = std::max(0.0f, jumpHoverRemaining_ - deltaTime);
                if (jumpHoverRemaining_ <= 0.0f)
                {
                    jumpStartedDescending_ = true;
                }
            }
            else
            {
                jumpStartedDescending_ = true;
            }
        }

        if (jumpStartedDescending_)
        {
            const float offsetX = jumpDestination_.x - position.x;
            if (std::abs(offsetX) > 0.0001f)
            {
                visual_.SetFacingFromHorizontalMovement(offsetX);
            }
            jumpVerticalVelocity_ -= JumpDropGravity * deltaTime;
            position.y += jumpVerticalVelocity_ * deltaTime;
            if (position.y <= GroundY)
            {
                position = jumpDestination_;
                jumpVerticalVelocity_ = 0.0f;
                if (!jumpImpactTriggered_)
                {
                    jumpImpactTriggered_ = true;
                    EmitChargeBurst(4, 1.0f, 1.0f);
                    SoundPlayer::Play(SlimeStampSoundPath);
                    if (triggerJumpImpactShake_)
                    {
                        triggerJumpImpactShake_();
                    }
                    if (spawnJumpShards_)
                    {
                        spawnJumpShards_(position, jumpShardAngle_);
                    }
                }
            }
        }

        if (jumpImpactTriggered_)
        {
            jumpRecoverRemaining_ = std::max(0.0f, jumpRecoverRemaining_ - deltaTime);
        }
    }
    else
    {
        HideTelegraph();
        HideRadialTelegraph();
    }

    const bool radialPatternFinished =
        activePattern_ != Pattern::RadialFloor ||
        (radialTelegraphRemaining_ <= 0.0f &&
            radialShakeRemaining_ <= 0.0f &&
            radialRecoverRemaining_ <= 0.0f);
    const bool jumpPatternFinished =
        activePattern_ != Pattern::JumpSlam ||
        (jumpImpactTriggered_ &&
            jumpRecoverRemaining_ <= 0.0f &&
            position.y <= GroundY + 0.0001f);

    if (patternElapsed_ >= currentPatternDuration_ &&
        radialPatternFinished &&
        jumpPatternFinished)
    {
        StartPattern(position);
    }

    position.x = std::max(-GroundLimitX, std::min(position.x, GroundLimitX));
    position.z = std::max(-GroundLimitZ, std::min(position.z, GroundLimitZ));
    GetOwner().GetTransform().SetPosition(position);
    visual_.SyncShadow();
}

void Slime5ControllerComponent::ReceiveHit(
    const DirectX::XMFLOAT2& direction, float knockbackSpeed)
{
    if (!health_.IsAlive())
    {
        BeginDying();
        return;
    }
    if (state_ != State::Acting) return;

    const float length = std::sqrt(direction.x * direction.x + direction.y * direction.y);
    if (length <= 0.0001f) return;

    knockbackVelocity_.x = direction.x / length * knockbackSpeed;
    knockbackVelocity_.y = direction.y / length * knockbackSpeed;
    knockbackTimeRemaining_ = KnockbackDuration;
    hitFlashTimeRemaining_ = HitFlashDuration;
    visual_.SetFlash(HitFlashColor, 1.0f);
}
