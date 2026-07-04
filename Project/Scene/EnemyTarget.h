#pragma once

class GameObject;
class HitReceiver;
class UnitHealthComponent;
class UnitHitboxComponent;

struct EnemyTarget
{
    GameObject* object = nullptr;
    UnitHitboxComponent* hitbox = nullptr;
    UnitHealthComponent* health = nullptr;
    HitReceiver* hitReceiver = nullptr;
};
