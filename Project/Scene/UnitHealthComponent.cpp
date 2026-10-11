#include "UnitHealthComponent.h"

#include <algorithm>

UnitHealthComponent::UnitHealthComponent(GameObject& owner, int maxHealth)
    : Component(owner)
    , maxHealth_(std::max(1, maxHealth))
    , currentHealth_(maxHealth_)
{
}

bool UnitHealthComponent::Damage(int amount)
{
    if (amount <= 0 || !CanTakeDamage()) return false;
    currentHealth_ = std::max(0, currentHealth_ - amount);
    return true;
}

void UnitHealthComponent::Reset()
{
    currentHealth_ = maxHealth_;
}

float UnitHealthComponent::GetHealthRatio() const
{
    return static_cast<float>(currentHealth_) / static_cast<float>(maxHealth_);
}
