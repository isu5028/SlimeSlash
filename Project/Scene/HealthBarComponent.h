#pragma once

#include "Component.h"

#include <array>
#include <chrono>
#include <memory>
#include <string>

class AsepriteSpriteComponent;
class GameObject;
class UnitHealthComponent;

class HealthBarComponent : public Component
{
public:
    HealthBarComponent(
        GameObject& owner,
        GameObject& target,
        UnitHealthComponent& health);

    bool Setup(
        Renderer& renderer,
        const std::string& filePath,
        float worldHeight,
        float verticalOffset);
    void Update() override;
    void Draw(Renderer& renderer) override;
    void SetFadeAlpha(float alpha);

private:
    GameObject& target_;
    UnitHealthComponent& health_;
    std::array<std::unique_ptr<AsepriteSpriteComponent>, 4> layers_;
    int previousHealth_ = 0;
    float damageTrailRatio_ = 1.0f;
    float damageTrailDelay_ = 0.0f;
    float fillStart_ = 0.0f;
    float fillEnd_ = 1.0f;
    float verticalOffset_ = 0.0f;
    float displayedHealthRatio_ = 1.0f;
    float fadeAlpha_ = 1.0f;
    float dissolveElapsed_ = 0.0f;
    bool dissolving_ = false;
    std::chrono::steady_clock::time_point previousUpdate_;
};
