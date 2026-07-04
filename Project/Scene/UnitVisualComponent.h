#pragma once

#include "Component.h"

#include <string>

class AsepriteSpriteComponent;
class ColoredPlaneComponent;
class UnitHitboxComponent;
class GameObject;

class UnitVisualComponent : public Component
{
public:
    UnitVisualComponent(
        GameObject& owner,
        GameObject& shadow,
        ColoredPlaneComponent& shadowRenderer,
        AsepriteSpriteComponent& sprite,
        float shadowWidth,
        float shadowDepth);

    void SetFacingFromHorizontalMovement(float movementX);
    void SetFlash(const DirectX::XMFLOAT4& color, float amount);
    void SetHitbox(UnitHitboxComponent& hitbox) { hitbox_ = &hitbox; }
    void SetHitboxVisible(bool visible);
    void SetVisible(bool visible);
    void SetDissolveAmount(float amount);
    void SetShadowVisible(bool visible);
    bool PlayAnimation(const std::string& tagName, bool restart = false);
    float GetAnimationDurationSeconds(const std::string& tagName) const;
    void SyncShadow();

private:
    GameObject& shadow_;
    ColoredPlaneComponent& shadowRenderer_;
    AsepriteSpriteComponent& sprite_;
    UnitHitboxComponent* hitbox_ = nullptr;
};
