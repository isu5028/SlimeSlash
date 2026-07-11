#include "HealthBarComponent.h"

#include "AsepriteSpriteComponent.h"
#include "GameObject.h"
#include "GameTime.h"
#include "UnitHealthComponent.h"

#include <algorithm>

namespace
{
    constexpr float CameraOffset = -0.05f;
    constexpr float TrailDelay = 0.18f;
    constexpr float TrailShrinkSpeed = 0.85f;
    constexpr float DissolveDuration = 0.65f;
}

HealthBarComponent::HealthBarComponent(
    GameObject& owner,
    GameObject& target,
    UnitHealthComponent& health)
    : Component(owner)
    , target_(target)
    , health_(health)
    , previousHealth_(health.GetCurrentHealth())
    , damageTrailRatio_(health.GetHealthRatio())
    , displayedHealthRatio_(health.GetHealthRatio())
    , previousUpdate_(std::chrono::steady_clock::now())
{
}

bool HealthBarComponent::Setup(
    Renderer& renderer,
    const std::string& filePath,
    float worldHeight,
    float verticalOffset)
{
    verticalOffset_ = verticalOffset;
    for (std::size_t index = 0; index < layers_.size(); ++index)
    {
        layers_[index] = std::make_unique<AsepriteSpriteComponent>(GetOwner());
        if (!layers_[index]->Setup(renderer, filePath, worldHeight) ||
            !layers_[index]->SetFrame(index)) return false;
    }
    if (!layers_[1]->GetOpaqueHorizontalBounds(1, fillStart_, fillEnd_))
    {
        fillStart_ = 0.0f;
        fillEnd_ = 1.0f;
    }
    return true;
}

void HealthBarComponent::Update()
{
    const auto now = std::chrono::steady_clock::now();
    float deltaTime = std::chrono::duration<float>(now - previousUpdate_).count();
    previousUpdate_ = now;
    if (GameTime::IsHitStopped()) return;
    deltaTime = std::min(deltaTime, 0.05f);

    const auto& targetPosition = target_.GetTransform().GetPosition();
    GetOwner().GetTransform().SetPosition({
        targetPosition.x,
        targetPosition.y + verticalOffset_,
        targetPosition.z + CameraOffset
    });

    const int currentHealth = health_.GetCurrentHealth();
    const float actualHealthRatio = health_.GetHealthRatio();
    if (currentHealth < previousHealth_)
    {
        const float previousRatio = static_cast<float>(previousHealth_) /
            static_cast<float>(health_.GetMaxHealth());
        damageTrailRatio_ = std::max(damageTrailRatio_, previousRatio);
        damageTrailDelay_ = TrailDelay;
        if (currentHealth == 0)
        {
            displayedHealthRatio_ = 0.0f;
            dissolving_ = true;
            dissolveElapsed_ = 0.0f;
        }
    }
    else if (currentHealth > 0 && currentHealth > previousHealth_)
    {
        dissolving_ = false;
        dissolveElapsed_ = 0.0f;
        displayedHealthRatio_ = actualHealthRatio;
        damageTrailRatio_ = actualHealthRatio;
        for (auto& layer : layers_) layer->SetDissolveAmount(0.0f);
    }
    previousHealth_ = currentHealth;

    if (!dissolving_)
    {
        displayedHealthRatio_ = actualHealthRatio;
    }

    const float healthRatio = displayedHealthRatio_;

    if (damageTrailDelay_ > 0.0f)
    {
        damageTrailDelay_ = std::max(0.0f, damageTrailDelay_ - deltaTime);
    }
    else
    {
        damageTrailRatio_ = std::max(
            healthRatio, damageTrailRatio_ - TrailShrinkSpeed * deltaTime);
    }

    if (dissolving_)
    {
        dissolveElapsed_ += deltaTime;
        const float dissolveAmount = std::min(dissolveElapsed_ / DissolveDuration, 1.0f);
        for (auto& layer : layers_) layer->SetDissolveAmount(dissolveAmount);
        if (dissolveAmount >= 1.0f) health_.SetBarVisible(false);
    }

    layers_[0]->SetHorizontalRange(0.0f, 1.0f);
    layers_[1]->SetHorizontalRange(0.0f, 1.0f);
    const float fillWidth = fillEnd_ - fillStart_;
    const float currentRight = fillStart_ + fillWidth * healthRatio;
    const float trailRight = fillStart_ + fillWidth * damageTrailRatio_;
    layers_[2]->SetHorizontalRange(currentRight, fillEnd_);
    layers_[3]->SetHorizontalRange(currentRight, trailRight);
}

void HealthBarComponent::Draw(Renderer& renderer)
{
    if (!health_.IsBarVisible()) return;
    for (auto& layer : layers_)
    {
        if (layer) layer->Draw(renderer);
    }
}

void HealthBarComponent::SetFadeAlpha(float alpha)
{
    fadeAlpha_ = std::max(0.0f, std::min(alpha, 1.0f));
    for (auto& layer : layers_)
    {
        if (layer) layer->SetOpacity(fadeAlpha_);
    }
}
