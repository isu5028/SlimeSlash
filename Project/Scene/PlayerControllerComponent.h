#pragma once

#include "Component.h"
#include "EnemyTarget.h"

#include <chrono>
#include <vector>

class GameObject;
class SlashAttackEffectComponent;
class AsepriteSpriteComponent;
class UnitVisualComponent;
class HitReceiver;
class UnitHitboxComponent;
class Camera;
class ImpactParticleComponent;
class UnitHealthComponent;

// キーボード入力による地上移動と、押下時間で高さが変わるジャンプを担当する。
class PlayerControllerComponent : public Component
{
public:
    PlayerControllerComponent(
        GameObject& owner,
        Camera& camera,
        ImpactParticleComponent& impactParticles,
        AsepriteSpriteComponent& sprite,
        UnitVisualComponent& visual,
        UnitHitboxComponent& hitbox,
        UnitHealthComponent& health,
        SlashAttackEffectComponent& attackEffect,
        std::vector<EnemyTarget>& attackTargets,
        float groundY);

    void Update() override;

private:
    enum class AnimationState
    {
        Idle,
        Walk,
        Jump,
        Fall,
        Attack,
        Damaged,
        Die,
    };

    bool IsKeyDown(int virtualKey) const;
    void PlayAnimation(AnimationState state);
    void BeginDeath();
    void StartAttack(int comboStage);
    bool TryHitAttackTargets(int comboStage);
    bool TryHitAttackTarget(EnemyTarget& target, int comboStage);
    bool TryTakeContactDamage(const DirectX::XMFLOAT3& position);

    AsepriteSpriteComponent& sprite_;
    Camera& camera_;
    ImpactParticleComponent& impactParticles_;
    UnitVisualComponent& visual_;
    UnitHitboxComponent& hitbox_;
    UnitHealthComponent& health_;
    SlashAttackEffectComponent& attackEffect_;
    std::vector<EnemyTarget>& attackTargets_;
    float groundY_ = 0.0f;
    float verticalVelocity_ = 0.0f;
    float jumpHoldTime_ = 0.0f;
    float attackAnimationTimeRemaining_ = 0.0f;
    float attackCooldownRemaining_ = 0.0f;
    float comboResetTimeRemaining_ = 0.0f;
    float damageAnimationTimeRemaining_ = 0.0f;
    float damageActionLockTimeRemaining_ = 0.0f;
    float damageFlashElapsed_ = 0.0f;
    float damageInvulnerabilityTimeRemaining_ = 0.0f;
    float damageKnockbackTimeRemaining_ = 0.0f;
    float deathAnimationTimeRemaining_ = 0.0f;
    float deathDissolveDelayRemaining_ = 0.0f;
    float dashElapsed_ = 0.0f;
    float dashSpinDirection_ = 1.0f;
    DirectX::XMFLOAT2 facingDirection_ = { 0.0f, 1.0f };
    DirectX::XMFLOAT2 dashDirection_ = { 0.0f, 1.0f };
    DirectX::XMFLOAT2 damageKnockbackVelocity_ = { 0.0f, 0.0f };
    AnimationState currentAnimationState_ = AnimationState::Idle;
    int nextAttackStage_ = 1;
    bool grounded_ = true;
    bool dashing_ = false;
    bool previousJumpKey_ = false;
    bool previousAttackKey_ = false;
    bool bufferedAttack_ = false;
    bool previousDashKey_ = false;
    bool airDashUsed_ = false;
    bool dead_ = false;
    std::chrono::steady_clock::time_point previousUpdate_;
};
