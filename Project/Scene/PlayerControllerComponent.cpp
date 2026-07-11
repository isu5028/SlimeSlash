#include "PlayerControllerComponent.h"

#include "AsepriteSpriteComponent.h"
#include "AttackSettings.h"
#include "Camera.h"
#include "GameTime.h"
#include "GameObject.h"
#include "HitReceiver.h"
#include "ImpactParticleComponent.h"
#include "SlashAttackEffectComponent.h"
#include "SoundPlayer.h"
#include "UnitVisualComponent.h"
#include "UnitHitboxComponent.h"
#include "UnitHealthComponent.h"

#include <Windows.h>
#include <algorithm>
#include <cmath>

namespace
{
    constexpr float MoveSpeed = 3.0f;
    constexpr float DashSpeed = 14.0f;
    constexpr float DashDuration = 0.18f;
    constexpr float DashCooldownDuration = 0.20f;
    constexpr float FirstAttackCooldown = 0.12f;
    constexpr float SecondAttackCooldown = 0.2f;
    constexpr float ThirdAttackCooldown = 0.28f;
    constexpr float ComboContinuationWindow = 0.4f;
    constexpr float DamageInvulnerabilityDuration = 0.75f;
    constexpr float DamageKnockbackDuration = 0.22f;
    constexpr float DamageKnockbackSpeed = 16.5f;
    constexpr float DamageFlashStepDuration = 0.12f;
    constexpr float DeathDissolveDelay = 2.0f;
    constexpr DirectX::XMFLOAT4 DamageFlashColor = {
        0.45490196f, 0.27843139f, 0.23921569f, 1.0f
    };
    constexpr float PlayerDamageShakeStrength = 0.24f;
    constexpr const wchar_t* AttackHitSoundPaths[] = {
        L"Project/Assets/Sounds/PlayerAttackHit.wav",
        L"Project/Assets/Sounds/PlayerAttackHit.wav",
        L"Project/Assets/Sounds/PlayerAttackHit.wav"
    };
    constexpr const wchar_t* AttackMissSoundPaths[] = {
        L"Project/Assets/Sounds/PlayerAttackMiss.wav",
        L"Project/Assets/Sounds/PlayerAttackMiss.wav",
        L"Project/Assets/Sounds/PlayerAttackMiss.wav"
    };
    // 強い初速と重力で、短時間に上昇・下降するゲームらしいジャンプにする。
    constexpr float InitialJumpVelocity = 10.f;
    constexpr float HeldJumpAcceleration = 3.0f;
    constexpr float MaximumJumpHoldTime = 0.12f;
    constexpr float JumpReleaseVelocityMultiplier = 0.35f;
    constexpr float RisingGravity = 24.0f;
    constexpr float FallingGravity = 44.0f;
    constexpr float GroundLimit = 4.5f;

    float DetermineDashSpinDirection(const DirectX::XMFLOAT2& direction)
    {
        if (std::abs(direction.x) > 0.25f)
        {
            return direction.x > 0.0f ? -1.0f : 1.0f;
        }

        return direction.y >= 0.0f ? -1.0f : 1.0f;
    }

    const wchar_t* GetAttackSoundPath(bool hit, int comboStage)
    {
        const int index = std::max(1, std::min(comboStage, 3)) - 1;
        return hit ? AttackHitSoundPaths[index] : AttackMissSoundPaths[index];
    }
}

PlayerControllerComponent::PlayerControllerComponent(
    GameObject& owner,
    InputManager& input,
    Camera& camera,
    ImpactParticleComponent& impactParticles,
    AsepriteSpriteComponent& sprite,
    UnitVisualComponent& visual,
    UnitHitboxComponent& hitbox,
    UnitHealthComponent& health,
    SlashAttackEffectComponent& attackEffect,
    std::vector<EnemyTarget>& attackTargets,
    float groundY)
    : Component(owner)
    , input_(input)
    , camera_(camera)
    , impactParticles_(impactParticles)
    , sprite_(sprite)
    , visual_(visual)
    , hitbox_(hitbox)
    , health_(health)
    , attackEffect_(attackEffect)
    , combatResolver_(attackTargets, impactParticles)
    , groundY_(groundY)
    , previousUpdate_(std::chrono::steady_clock::now())
{
}

bool PlayerControllerComponent::HasFullyDisappearedAfterDeath() const
{
    return dead_ &&
        deathDissolveDelayRemaining_ <= 0.0f &&
        deathAnimationTimeRemaining_ <= 0.0f;
}

bool PlayerControllerComponent::IsDeathDissolving() const
{
    return dead_ && deathDissolveDelayRemaining_ <= 0.0f;
}

void PlayerControllerComponent::PlayAnimation(AnimationState state)
{
    if (currentAnimationState_ == state)
    {
        return;
    }

    bool played = false;
    switch (state)
    {
    case AnimationState::Idle:
        played = sprite_.Play("Idle");
        break;
    case AnimationState::Walk:
        played = sprite_.Play("Walk");
        break;
    case AnimationState::Jump:
        played = sprite_.Play("Jump");
        break;
    case AnimationState::Fall:
        played = sprite_.Play("Fall");
        break;
    case AnimationState::Attack:
        played = sprite_.Play("Attack", true);
        break;
    case AnimationState::Damaged:
        played = sprite_.Play("Damaged", true);
        break;
    case AnimationState::Die:
        played = sprite_.Play("Die", true);
        break;
    }

    if (played) currentAnimationState_ = state;
}

void PlayerControllerComponent::BeginDeath()
{
    if (dead_) return;

    dead_ = true;
    health_.SetCanTakeDamage(false);
    visual_.SetVisible(true);
    visual_.SetHitboxVisible(false);
    visual_.SetFlash(DamageFlashColor, 0.0f);
    visual_.SetDissolveAmount(0.0f);
    attackAnimationTimeRemaining_ = 0.0f;
    damageKnockbackTimeRemaining_ = 0.0f;
    damageAnimationTimeRemaining_ = 0.0f;
    damageActionLockTimeRemaining_ = 0.0f;
    bufferedAttack_ = false;
    nextAttackStage_ = 1;
    comboResetTimeRemaining_ = 0.0f;
    dashing_ = false;
    dashCooldownRemaining_ = 0.0f;
    bufferedDash_ = false;

    deathAnimationTimeRemaining_ = std::max(
        sprite_.GetAnimationDurationSeconds("Die"), 0.1f);
    deathDissolveDelayRemaining_ = DeathDissolveDelay;
    if (sprite_.Play("Die", true))
    {
        currentAnimationState_ = AnimationState::Die;
    }
}

bool PlayerControllerComponent::ApplyDamageReaction(
    int damage,
    const DirectX::XMFLOAT2& knockbackDirection,
    const DirectX::XMFLOAT3& impactPosition)
{
    if (damage <= 0 || dead_ || damageInvulnerabilityTimeRemaining_ > 0.0f) return false;

    float directionX = knockbackDirection.x;
    float directionZ = knockbackDirection.y;
    const float length = std::sqrt(directionX * directionX + directionZ * directionZ);
    if (length > 0.0001f)
    {
        directionX /= length;
        directionZ /= length;
    }
    else
    {
        directionX = -facingDirection_.x;
        directionZ = -facingDirection_.y;
    }

    if (!health_.Damage(damage)) return false;
    const bool lethalDamage = !health_.IsAlive();

    damageKnockbackVelocity_ = {
        directionX * DamageKnockbackSpeed,
        directionZ * DamageKnockbackSpeed
    };
    SoundPlayer::Play(L"Project/Assets/Sounds/PlayerDamaged.wav");
    impactParticles_.Emit(
        impactPosition, { directionX, directionZ }, true,
        { 1.0f, 1.0f, 1.0f, 1.0f });
    camera_.TriggerShake(
        { directionX, directionZ }, PlayerDamageShakeStrength, 0.40f, true);
    GameTime::StartHitStop(0.10f);
    attackAnimationTimeRemaining_ = 0.0f;
    bufferedAttack_ = false;
    nextAttackStage_ = 1;
    comboResetTimeRemaining_ = 0.0f;
    dashing_ = false;
    dashCooldownRemaining_ = 0.0f;
    bufferedDash_ = false;
    if (lethalDamage)
    {
        BeginDeath();
    }
    else
    {
        damageKnockbackTimeRemaining_ = DamageKnockbackDuration;
        damageInvulnerabilityTimeRemaining_ = DamageInvulnerabilityDuration;
        damageAnimationTimeRemaining_ = sprite_.GetAnimationDurationSeconds("Damaged");
        damageActionLockTimeRemaining_ = damageAnimationTimeRemaining_ * 0.25f;
        damageFlashElapsed_ = 0.0f;
        visual_.SetFlash(DamageFlashColor, 1.0f);
        sprite_.Play("Damaged", true);
        currentAnimationState_ = AnimationState::Damaged;
    }
    return true;
}

bool PlayerControllerComponent::ReceiveHazardDamage(
    const DirectX::XMFLOAT2& knockbackDirection,
    int damage,
    const DirectX::XMFLOAT3& impactPosition)
{
    if (dashing_) return false;
    return ApplyDamageReaction(damage, knockbackDirection, impactPosition);
}

void PlayerControllerComponent::SetInputEnabled(bool enabled)
{
    inputEnabled_ = enabled;
    if (!inputEnabled_)
    {
        bufferedAttack_ = false;
        bufferedDash_ = false;
    }
}

void PlayerControllerComponent::BeginStageIntro(
    const DirectX::XMFLOAT3& startPosition,
    const DirectX::XMFLOAT3& landingPosition,
    float duration,
    float arcHeight)
{
    stageIntroActive_ = true;
    stageIntroStart_ = startPosition;
    stageIntroLanding_ = landingPosition;
    stageIntroDuration_ = std::max(duration, 0.0001f);
    stageIntroElapsed_ = 0.0f;
    stageIntroArcHeight_ = std::max(arcHeight, 0.0f);
    grounded_ = false;
    dashing_ = false;
    verticalVelocity_ = 0.0f;
    jumpHoldTime_ = 0.0f;
    airDashUsed_ = false;
    attackAnimationTimeRemaining_ = 0.0f;
    damageKnockbackTimeRemaining_ = 0.0f;
    GetOwner().GetTransform().SetPosition(startPosition);

    const float moveX = landingPosition.x - startPosition.x;
    if (std::abs(moveX) > 0.0001f)
    {
        visual_.SetFacingFromHorizontalMovement(moveX);
    }
}

void PlayerControllerComponent::StartDash()
{
    SoundPlayer::Play(L"Project/Assets/Sounds/Dash.wav");
    if (!grounded_) airDashUsed_ = true;
    dashing_ = true;
    bufferedDash_ = false;
    dashCooldownRemaining_ = 0.0f;
    dashElapsed_ = 0.0f;
    verticalVelocity_ = 0.0f;
    dashDirection_ = facingDirection_;
    dashSpinDirection_ = DetermineDashSpinDirection(dashDirection_);
    visual_.SetFacingFromHorizontalMovement(dashDirection_.x);
}

void PlayerControllerComponent::StartAttack(int comboStage)
{
    damageAnimationTimeRemaining_ = 0.0f;
    attackEffect_.Trigger(facingDirection_, comboStage);
    const bool hit = combatResolver_.ResolveAttack(
        GetOwner().GetTransform().GetPosition(), facingDirection_, comboStage, camera_);
    SoundPlayer::Play(GetAttackSoundPath(hit, comboStage));
    attackAnimationTimeRemaining_ = sprite_.GetAnimationDurationSeconds("Attack");
    sprite_.Play("Attack", true);
    currentAnimationState_ = AnimationState::Attack;
    bufferedAttack_ = false;

    if (comboStage == 1)
    {
        attackCooldownRemaining_ = FirstAttackCooldown;
        comboResetTimeRemaining_ = FirstAttackCooldown + ComboContinuationWindow;
        nextAttackStage_ = 2;
    }
    else if (comboStage == 2)
    {
        attackCooldownRemaining_ = SecondAttackCooldown;
        comboResetTimeRemaining_ = SecondAttackCooldown + ComboContinuationWindow;
        nextAttackStage_ = 3;
    }
    else
    {
        attackCooldownRemaining_ = ThirdAttackCooldown;
        comboResetTimeRemaining_ = 0.0f;
        nextAttackStage_ = 1;
    }
}

bool PlayerControllerComponent::TryTakeContactDamage(const DirectX::XMFLOAT3& position)
{
    if (dashing_) return false;
    if (damageInvulnerabilityTimeRemaining_ > 0.0f) return false;
    PlayerCombatResolver::Contact contact;
    if (!combatResolver_.FindContact(position, facingDirection_, hitbox_, contact)) return false;
    auto impactPosition = position;
    impactPosition.y += hitbox_.GetHeight() * 0.55f;
    return ApplyDamageReaction(2, contact.knockbackDirection, impactPosition);
}

void PlayerControllerComponent::Update()
{
    const auto now = std::chrono::steady_clock::now();
    float deltaTime = std::chrono::duration<float>(now - previousUpdate_).count();
    previousUpdate_ = now;
    if (GameTime::IsHitStopped()) return;

    if (!dead_ && !health_.IsAlive()) BeginDeath();

    // ブレークポイント復帰などで巨大な移動量が発生しないよう、1フレームの時間を制限する。
    deltaTime = std::min(deltaTime, 0.05f);

    if (dead_)
    {
        if (deathDissolveDelayRemaining_ > 0.0f)
        {
            deathDissolveDelayRemaining_ = std::max(
                0.0f, deathDissolveDelayRemaining_ - deltaTime);
            visual_.SetDissolveAmount(0.0f);
            return;
        }

        deathAnimationTimeRemaining_ = std::max(
            0.0f, deathAnimationTimeRemaining_ - deltaTime);
        const float deathDuration = std::max(
            sprite_.GetAnimationDurationSeconds("Die"), 0.1f);
        visual_.SetDissolveAmount(
            1.0f - deathAnimationTimeRemaining_ / deathDuration);
        if (deathAnimationTimeRemaining_ <= 0.0f) visual_.SetVisible(false);
        return;
    }

    if (stageIntroActive_)
    {
        stageIntroElapsed_ = std::min(stageIntroElapsed_ + deltaTime, stageIntroDuration_);
        const float t = stageIntroElapsed_ / stageIntroDuration_;
        auto position = stageIntroStart_;
        position.x += (stageIntroLanding_.x - stageIntroStart_.x) * t;
        position.z += (stageIntroLanding_.z - stageIntroStart_.z) * t;
        position.y = stageIntroLanding_.y + stageIntroArcHeight_ * 4.0f * t * (1.0f - t);
        auto rotation = GetOwner().GetTransform().GetRotation();
        rotation.z = 0.0f;
        GetOwner().GetTransform().SetPosition(position);
        GetOwner().GetTransform().SetRotation(rotation);
        visual_.SyncShadow();

        if (t < 0.5f)
        {
            PlayAnimation(AnimationState::Jump);
        }
        else
        {
            PlayAnimation(AnimationState::Fall);
        }

        if (stageIntroElapsed_ >= stageIntroDuration_)
        {
            stageIntroActive_ = false;
            grounded_ = true;
            verticalVelocity_ = 0.0f;
            GetOwner().GetTransform().SetPosition(stageIntroLanding_);
            visual_.SyncShadow();
            PlayAnimation(AnimationState::Idle);
        }

        return;
    }

    attackAnimationTimeRemaining_ =
        std::max(0.0f, attackAnimationTimeRemaining_ - deltaTime);
    dashCooldownRemaining_ =
        std::max(0.0f, dashCooldownRemaining_ - deltaTime);
    attackCooldownRemaining_ =
        std::max(0.0f, attackCooldownRemaining_ - deltaTime);
    comboResetTimeRemaining_ =
        std::max(0.0f, comboResetTimeRemaining_ - deltaTime);
    damageAnimationTimeRemaining_ =
        std::max(0.0f, damageAnimationTimeRemaining_ - deltaTime);
    damageActionLockTimeRemaining_ =
        std::max(0.0f, damageActionLockTimeRemaining_ - deltaTime);
    damageInvulnerabilityTimeRemaining_ =
        std::max(0.0f, damageInvulnerabilityTimeRemaining_ - deltaTime);
    damageKnockbackTimeRemaining_ =
        std::max(0.0f, damageKnockbackTimeRemaining_ - deltaTime);

    if (damageActionLockTimeRemaining_ > 0.0f)
    {
        damageFlashElapsed_ += deltaTime;
        const int flashStep = static_cast<int>(damageFlashElapsed_ / DamageFlashStepDuration);
        visual_.SetFlash(DamageFlashColor, flashStep % 2 == 0 ? 1.0f : 0.0f);
    }
    else
    {
        visual_.SetFlash(DamageFlashColor, 0.0f);
    }

    const bool beingKnockedBack = damageKnockbackTimeRemaining_ > 0.0f;

    auto position = GetOwner().GetTransform().GetPosition();
    auto rotation = GetOwner().GetTransform().GetRotation();

    // WASDと矢印キーを同じ方向入力として扱う。
    float moveX = 0.0f;
    float moveZ = 0.0f;
    if (inputEnabled_)
    {
        if (input_.IsAnyDown('A', VK_LEFT))  moveX -= 1.0f;
        if (input_.IsAnyDown('D', VK_RIGHT)) moveX += 1.0f;
        if (input_.IsAnyDown('W', VK_UP))    moveZ += 1.0f;
        if (input_.IsAnyDown('S', VK_DOWN))  moveZ -= 1.0f;
    }

    // 斜め移動だけ速くならないよう、方向ベクトルを正規化する。
    const float moveLength = std::sqrt(moveX * moveX + moveZ * moveZ);
    if (!dashing_ && !beingKnockedBack && moveLength > 0.0f)
    {
        moveX /= moveLength;
        moveZ /= moveLength;

        facingDirection_ = { moveX, moveZ };
        position.x += moveX * MoveSpeed * deltaTime;
        position.z += moveZ * MoveSpeed * deltaTime;
    }

    // Xスケールの符号でSpriteを左右反転する。
    // 左右入力がない間は、最後に向いていた方向を維持する。
    if (!dashing_ && !beingKnockedBack && moveX != 0.0f)
    {
        visual_.SetFacingFromHorizontalMovement(moveX);
    }

    // 10x10の地面からプレイヤーが外へ出ないようにする。
    const bool jumpKey = inputEnabled_ && input_.IsDown(VK_SPACE);
    const bool dashPressed = inputEnabled_ && input_.WasAnyPressed(VK_SHIFT, 'K');

    const bool canStartDash = grounded_ || !airDashUsed_;
    if (dashPressed && !dashing_ && !beingKnockedBack && canStartDash)
    {
        if (dashCooldownRemaining_ <= 0.0f)
        {
            StartDash();
        }
        else
        {
            bufferedDash_ = true;
        }
    }

    if (dashing_)
    {
        dashElapsed_ += deltaTime;
        position.x += dashDirection_.x * DashSpeed * deltaTime;
        position.z += dashDirection_.y * DashSpeed * deltaTime;

        const float dashProgress = std::min(dashElapsed_ / DashDuration, 1.0f);
        rotation.z = dashSpinDirection_ * DirectX::XM_2PI * dashProgress;

        if (dashElapsed_ >= DashDuration)
        {
            dashing_ = false;
            dashElapsed_ = 0.0f;
            dashCooldownRemaining_ = DashCooldownDuration;
            rotation.z = 0.0f;
        }
    }
    else
    {
        rotation.z = 0.0f;
    }

    if (!dashing_ && bufferedDash_ && dashCooldownRemaining_ <= 0.0f &&
        !beingKnockedBack && canStartDash)
    {
        StartDash();
    }

    if (beingKnockedBack)
    {
        const float knockbackScale =
            damageKnockbackTimeRemaining_ / DamageKnockbackDuration;
        position.x += damageKnockbackVelocity_.x * knockbackScale * deltaTime;
        position.z += damageKnockbackVelocity_.y * knockbackScale * deltaTime;
    }

    // Spaceを押した瞬間だけジャンプを開始する。着地までは再ジャンプできない。
    if (inputEnabled_ && input_.WasPressed(VK_SPACE) &&
        grounded_ && !dashing_ && !beingKnockedBack)
    {
        SoundPlayer::Play(L"Project/Assets/Sounds/Jump.wav");
        grounded_ = false;
        verticalVelocity_ = InitialJumpVelocity;
        jumpHoldTime_ = 0.0f;
    }

    // 上昇中にSpaceを押し続けると、最大時間まで上向きの力を追加する。
    // 早く離すほど追加加速が短くなるため、ジャンプが低くなる。
    if (!grounded_ && !dashing_ && jumpKey && verticalVelocity_ > 0.0f &&
        jumpHoldTime_ < MaximumJumpHoldTime)
    {
        verticalVelocity_ += HeldJumpAcceleration * deltaTime;
        jumpHoldTime_ += deltaTime;
    }

    // Releasing early sharply cuts upward momentum, making tap and hold jumps distinct.
    if (!grounded_ && !dashing_ && inputEnabled_ && input_.WasReleased(VK_SPACE) &&
        verticalVelocity_ > 0.0f && jumpHoldTime_ < MaximumJumpHoldTime)
    {
        verticalVelocity_ *= JumpReleaseVelocityMultiplier;
    }

    if (!grounded_ && !dashing_)
    {
        // 下降時は重力をさらに強くし、ふわっとせず素早く着地させる。
        const float gravity = verticalVelocity_ > 0.0f ? RisingGravity : FallingGravity;
        verticalVelocity_ -= gravity * deltaTime;
        position.y += verticalVelocity_ * deltaTime;

        // 地面より下へ進んだら、位置と速度を着地状態へ戻す。
        if (position.y <= groundY_)
        {
            position.y = groundY_;
            verticalVelocity_ = 0.0f;
            grounded_ = true;
            airDashUsed_ = false;
        }
    }

    position.x = std::max(-GroundLimit, std::min(position.x, GroundLimit));
    position.z = std::max(-GroundLimit, std::min(position.z, GroundLimit));

    if (TryTakeContactDamage(position)) rotation.z = 0.0f;

    GetOwner().GetTransform().SetPosition(position);
    GetOwner().GetTransform().SetRotation(rotation);

    // 影はジャンプせず、地面上でプレイヤーの真下（同じX/Z）を追従する。
    visual_.SyncShadow();

    // Lethal contact starts Die above; normal animation selection must not overwrite it.
    if (dead_) return;

    const bool attackPressed = inputEnabled_ && input_.WasAnyPressed('J', 'Z');
    if (attackPressed && damageActionLockTimeRemaining_ <= 0.0f)
    {
        if (attackCooldownRemaining_ <= 0.0f)
        {
            StartAttack(nextAttackStage_);
        }
        else if (nextAttackStage_ != 1)
        {
            bufferedAttack_ = true;
        }
    }

    if (bufferedAttack_ && attackCooldownRemaining_ <= 0.0f &&
        damageActionLockTimeRemaining_ <= 0.0f)
    {
        StartAttack(nextAttackStage_);
    }
    else if (nextAttackStage_ != 1 && comboResetTimeRemaining_ <= 0.0f)
    {
        nextAttackStage_ = 1;
        bufferedAttack_ = false;
    }

    if (damageAnimationTimeRemaining_ > 0.0f)
    {
        PlayAnimation(AnimationState::Damaged);
    }
    else if (attackAnimationTimeRemaining_ > 0.0f)
    {
        PlayAnimation(AnimationState::Attack);
    }
    else if (!grounded_)
    {
        if (verticalVelocity_ > 0.0f)
        {
            PlayAnimation(AnimationState::Jump);
        }
        else
        {
            PlayAnimation(AnimationState::Fall);
        }
    }
    else if (!dashing_ && !beingKnockedBack && moveLength > 0.0f)
    {
        PlayAnimation(AnimationState::Walk);
    }
    else
    {
        PlayAnimation(AnimationState::Idle);
    }

}
