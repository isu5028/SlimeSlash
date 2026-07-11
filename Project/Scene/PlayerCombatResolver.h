#pragma once

#include "EnemyTarget.h"

#include <vector>

class Camera;
class ImpactParticleComponent;
class UnitHitboxComponent;

class PlayerCombatResolver
{
public:
    struct Contact
    {
        DirectX::XMFLOAT2 knockbackDirection = {};
    };

    PlayerCombatResolver(
        std::vector<EnemyTarget>& targets,
        ImpactParticleComponent& impactParticles);

    bool ResolveAttack(
        const DirectX::XMFLOAT3& playerPosition,
        const DirectX::XMFLOAT2& facingDirection,
        int comboStage,
        Camera& camera) const;
    bool FindContact(
        const DirectX::XMFLOAT3& playerPosition,
        const DirectX::XMFLOAT2& fallbackDirection,
        const UnitHitboxComponent& playerHitbox,
        Contact& contact) const;

private:
    bool ResolveTarget(
        EnemyTarget& target,
        const DirectX::XMFLOAT3& playerPosition,
        const DirectX::XMFLOAT2& facingDirection,
        int comboStage,
        Camera& camera) const;

    std::vector<EnemyTarget>& targets_;
    ImpactParticleComponent& impactParticles_;
};
