#pragma once

#include "Component.h"
#include "EnemyController.h"

#include <chrono>
#include <cstdint>
#include <functional>
#include <vector>

class GameObject;
class PlayerControllerComponent;
class UnitHealthComponent;
class UnitVisualComponent;
class ChargeParticleComponent;
class ColoredDiskComponent;
class ColoredPlaneComponent;

class Slime5ControllerComponent : public Component, public EnemyController
{
public:
    Slime5ControllerComponent(
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
        std::function<void()> triggerJumpImpactShake);

    void Update() override;
    void ReceiveHit(const DirectX::XMFLOAT2& direction, float knockbackSpeed) override;
    void StartSpawn(int spawnIndex) override;
    bool IsDefeated() const override { return state_ == State::Hidden; }

private:
    enum class State
    {
        Acting,
        Dying,
        Escaping,
        Hidden,
        Spawning,
    };

    enum class Pattern
    {
        Chase,
        Dash,
        RadialFloor,
        Strafe,
        JumpSlam,
    };

    void BeginDying();
    void BeginEscape();
    void EmitChargeBurst(int count, float speedScale, float sizeScale);
    DirectX::XMFLOAT2 GetDirectionAwayFromPlayer();
    void HideTelegraph();
    void HideRadialTelegraph();
    Pattern ChooseNextPattern();
    static const char* PatternToString(Pattern pattern);
    void QueueRadialDamageFloors(const DirectX::XMFLOAT3& position);
    void StartPattern(const DirectX::XMFLOAT3& position);
    void SnapDashDirectionToPlayer(const DirectX::XMFLOAT3& position);
    void UpdateTelegraph(
        const DirectX::XMFLOAT3& position,
        const DirectX::XMFLOAT2& direction,
        float lengthScale);
    void UpdateRadialTelegraph(const DirectX::XMFLOAT3& position, float radiusScale);
    void StartJumpSlam(const DirectX::XMFLOAT3& position);
    DirectX::XMFLOAT3 ClampJumpDestination(const DirectX::XMFLOAT3& position) const;

    GameObject& target_;
    PlayerControllerComponent& targetController_;
    UnitVisualComponent& visual_;
    UnitHealthComponent& health_;
    ChargeParticleComponent& chargeParticles_;
    GameObject& telegraphArea_;
    ColoredPlaneComponent& telegraphRenderer_;
    GameObject& radialTelegraphArea_;
    ColoredDiskComponent& radialTelegraphRenderer_;
    std::function<void(const DirectX::XMFLOAT3&, float)> queueDamageFloorSpawn_;
    std::function<void(const DirectX::XMFLOAT3&, float)> spawnJumpShards_;
    std::function<void(const DirectX::XMFLOAT3&)> spawnDeathHitEffect_;
    std::function<void()> triggerJumpImpactShake_;
    State state_ = State::Acting;
    Pattern activePattern_ = Pattern::Chase;
    Pattern previousPattern_ = Pattern::Chase;
    bool hasPreviousPattern_ = false;
    float stateElapsed_ = 0.0f;
    float patternElapsed_ = 0.0f;
    float hitFlashTimeRemaining_ = 0.0f;
    float knockbackTimeRemaining_ = 0.0f;
    float dashWindupRemaining_ = 0.0f;
    float dashPauseRemaining_ = 0.0f;
    float dashDurationRemaining_ = 0.0f;
    float radialTelegraphRemaining_ = 0.0f;
    float radialShakeRemaining_ = 0.0f;
    float radialRecoverRemaining_ = 0.0f;
    float jumpHoverRemaining_ = 0.0f;
    float jumpRecoverRemaining_ = 0.0f;
    float currentPatternDuration_ = 0.0f;
    float jumpVerticalVelocity_ = 0.0f;
    float jumpTravelElapsed_ = 0.0f;
    float jumpTravelDuration_ = 0.0f;
    float deathEffectTimer_ = 0.0f;
    bool jumpStartedDescending_ = false;
    bool jumpImpactTriggered_ = false;
    float jumpShardAngle_ = 0.0f;
    DirectX::XMFLOAT2 jumpHorizontalVelocity_ = { 0.0f, 0.0f };
    DirectX::XMFLOAT2 knockbackVelocity_ = { 0.0f, 0.0f };
    DirectX::XMFLOAT2 dashDirection_ = { 0.0f, 1.0f };
    DirectX::XMFLOAT2 strafeDirection_ = { 1.0f, 0.0f };
    DirectX::XMFLOAT3 telegraphAnchorPosition_ = {};
    DirectX::XMFLOAT3 spawnStart_ = {};
    DirectX::XMFLOAT3 spawnEnd_ = {};
    DirectX::XMFLOAT3 escapeStart_ = {};
    DirectX::XMFLOAT3 escapeEnd_ = {};
    DirectX::XMFLOAT3 radialCenterPosition_ = {};
    DirectX::XMFLOAT3 jumpStartPosition_ = {};
    DirectX::XMFLOAT3 jumpDestination_ = {};
    DirectX::XMFLOAT3 deathBasePosition_ = {};
    float spawnDelayRemaining_ = 0.0f;
    float deathDuration_ = 0.0f;
    float deathShakeStrength_ = 0.0f;
    int deathEffectsRemaining_ = 0;
    bool radialFloorUnlocked_ = false;
    bool radialFloorGuaranteedPending_ = false;
    bool radialFloorRecoveryPending_ = false;
    std::uint32_t randomState_ = 0x5f3759dfu;
    std::chrono::steady_clock::time_point previousUpdate_;
};
