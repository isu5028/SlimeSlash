#include "PlayerCombatResolver.h"

#include "AttackSettings.h"
#include "Camera.h"
#include "GameObject.h"
#include "GameTime.h"
#include "HitReceiver.h"
#include "ImpactParticleComponent.h"
#include "UnitHealthComponent.h"
#include "UnitHitboxComponent.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr float NormalHitShakeStrength = 0.08f;
    constexpr float StrongHitShakeStrength = 0.18f;
}

PlayerCombatResolver::PlayerCombatResolver(
    std::vector<EnemyTarget>& targets,
    ImpactParticleComponent& impactParticles)
    : targets_(targets)
    , impactParticles_(impactParticles)
{
}

bool PlayerCombatResolver::ResolveAttack(
    const DirectX::XMFLOAT3& playerPosition,
    const DirectX::XMFLOAT2& facingDirection,
    int comboStage,
    Camera& camera) const
{
    // 攻撃対象の一覧はEnemyTargetRegistryが所有し、Resolverは判定だけを担当する
    bool hit = false;
    for (auto& target : targets_)
    {
        hit = ResolveTarget(target, playerPosition, facingDirection, comboStage, camera) || hit;
    }
    return hit;
}

bool PlayerCombatResolver::ResolveTarget(
    EnemyTarget& target,
    const DirectX::XMFLOAT3& playerPosition,
    const DirectX::XMFLOAT2& facingDirection,
    int comboStage,
    Camera& camera) const
{
    // TargetRegistryには敵・ダメージ床・飛翔破片などが入る
    // ここでは「攻撃を受けられる対象」だけを距離判定の対象にする
    if (!target.object || !target.hitbox || !target.health ||
        !target.hitReceiver || !target.health->CanTakeDamage()) return false;

    const auto& targetPosition = target.object->GetTransform().GetPosition();
    const float offsetX = targetPosition.x - playerPosition.x;
    const float offsetZ = targetPosition.z - playerPosition.z;
    const float distance = std::sqrt(offsetX * offsetX + offsetZ * offsetZ);
    const float distanceToHitbox = std::max(
        0.0f, distance - target.hitbox->GetDirectionalRadius(offsetX, offsetZ));
    if (distance <= 0.0001f ||
        distanceToHitbox > AttackSettings::GetHitRange(comboStage)) return false;

    const float targetDirectionX = offsetX / distance;
    const float targetDirectionZ = offsetZ / distance;
    const float facingDot =
        targetDirectionX * facingDirection.x + targetDirectionZ * facingDirection.y;
    if (facingDot < AttackSettings::GetHitMinimumDot(comboStage)) return false;

    const float attackBottom = playerPosition.y + AttackSettings::HitBottomOffset;
    const float attackTop = playerPosition.y + AttackSettings::HitTopOffset;
    const float targetBottom = targetPosition.y;
    const float targetTop = targetPosition.y + target.hitbox->GetHeight();
    if (attackTop < targetBottom || attackBottom > targetTop) return false;

    const int damage = comboStage == 1 ? 2 : comboStage == 2 ? 3 : 5;
    // Hitboxは形状を提供するだけで、ダメージ適用と被弾リアクションはここで分担する
    if (!target.health->Damage(damage)) return false;

    const float knockbackSpeed = comboStage == 1 ? 2.f : comboStage == 2 ? 3.f : 8.f;
    target.hitReceiver->ReceiveHit(facingDirection, knockbackSpeed);
    if (target.emitsHitEffect)
    {
        auto impactPosition = targetPosition;
        impactPosition.y += target.hitbox->GetHeight() * 0.55f;
        impactParticles_.Emit(
            impactPosition, facingDirection, comboStage == 3,
            target.hitEffectColor, target.hitEffectSizeScale,
            target.hitEffectCountOverride);
    }
    if (comboStage == 3)
    {
        camera.TriggerShake(facingDirection, StrongHitShakeStrength, 0.34f, true);
        GameTime::StartHitStop(0.16f);
    }
    else
    {
        camera.TriggerShake(facingDirection, NormalHitShakeStrength, 0.08f);
    }
    return true;
}

bool PlayerCombatResolver::FindContact(
    const DirectX::XMFLOAT3& playerPosition,
    const DirectX::XMFLOAT2& fallbackDirection,
    const UnitHitboxComponent& playerHitbox,
    Contact& contact) const
{
    EnemyTarget* contactTarget = nullptr;
    float contactDistanceSquared = 0.0f;
    for (auto& target : targets_)
    {
        if (!target.object || !target.hitbox || !target.health ||
            !target.health->CanTakeDamage() || !target.causesContactDamage) continue;
        auto* contactHitbox = target.contactHitbox ? target.contactHitbox : target.hitbox;
        const auto& targetPosition = target.object->GetTransform().GetPosition();
        const float offsetX = playerPosition.x - targetPosition.x;
        const float offsetZ = playerPosition.z - targetPosition.z;
        const float scaledOffsetZ = offsetZ / UnitHitboxComponent::DepthRadiusScale;
        const float distanceSquared = offsetX * offsetX + scaledOffsetZ * scaledOffsetZ;
        const float contactRadius = playerHitbox.GetRadius() + contactHitbox->GetRadius();
        const float playerTop = playerPosition.y + playerHitbox.GetHeight();
        const float targetTop = targetPosition.y + contactHitbox->GetHeight();
        if (distanceSquared > contactRadius * contactRadius ||
            playerTop < targetPosition.y || playerPosition.y > targetTop) continue;
        if (!contactTarget || distanceSquared < contactDistanceSquared)
        {
            contactTarget = &target;
            contactDistanceSquared = distanceSquared;
        }
    }
    if (!contactTarget) return false;

    const auto& targetPosition = contactTarget->object->GetTransform().GetPosition();
    const float offsetX = playerPosition.x - targetPosition.x;
    const float offsetZ = playerPosition.z - targetPosition.z;
    const float distance = std::sqrt(contactDistanceSquared);
    contact.knockbackDirection = { -fallbackDirection.x, -fallbackDirection.y };
    if (distance > 0.0001f)
    {
        contact.knockbackDirection = { offsetX / distance, offsetZ / distance };
    }
    return true;
}
