#pragma once

#include "Component.h"

class UnitHealthComponent : public Component
{
public:
    UnitHealthComponent(GameObject& owner, int maxHealth);

    bool Damage(int amount);
    void Reset();
    int GetCurrentHealth() const { return currentHealth_; }
    int GetMaxHealth() const { return maxHealth_; }
    float GetHealthRatio() const;
    bool IsAlive() const { return currentHealth_ > 0; }
    bool CanTakeDamage() const { return canTakeDamage_ && IsAlive(); }
    void SetCanTakeDamage(bool value) { canTakeDamage_ = value; }
    bool IsBarVisible() const { return barVisible_; }
    void SetBarVisible(bool visible) { barVisible_ = visible; }

private:
    int maxHealth_ = 1;
    int currentHealth_ = 1;
    bool canTakeDamage_ = true;
    bool barVisible_ = true;
};
