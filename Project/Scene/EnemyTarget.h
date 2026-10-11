#pragma once

class GameObject;
class HitReceiver;
class UnitHealthComponent;
class UnitHitboxComponent;

struct EnemyTarget
{
    // Registryに登録する戦闘対象の参照情報
    // GameObjectを所有せず、各Componentを借用する
    GameObject* object = nullptr;
    UnitHitboxComponent* hitbox = nullptr;
    UnitHitboxComponent* contactHitbox = nullptr;
    UnitHealthComponent* health = nullptr;
    HitReceiver* hitReceiver = nullptr;
    bool causesContactDamage = true;
    bool emitsHitEffect = true;
    DirectX::XMFLOAT4 hitEffectColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    float hitEffectSizeScale = 1.0f;
    int hitEffectCountOverride = -1;
};
