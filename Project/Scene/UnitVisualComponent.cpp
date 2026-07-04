#include "UnitVisualComponent.h"

#include "AsepriteSpriteComponent.h"
#include "ColoredPlaneComponent.h"
#include "GameObject.h"
#include "UnitHitboxComponent.h"

UnitVisualComponent::UnitVisualComponent(
    GameObject& owner,
    GameObject& shadow,
    ColoredPlaneComponent& shadowRenderer,
    AsepriteSpriteComponent& sprite,
    float shadowWidth,
    float shadowDepth)
    : Component(owner)
    , shadow_(shadow)
    , shadowRenderer_(shadowRenderer)
    , sprite_(sprite)
{
    shadow_.GetTransform().SetScale({ shadowWidth, 1.0f, shadowDepth });
    constexpr float SpritePixelWorldSize = 2.0f / 32.0f;
    shadowRenderer_.SetDissolvePixelSize(
        SpritePixelWorldSize / shadowWidth,
        SpritePixelWorldSize / shadowDepth);
}

void UnitVisualComponent::SetVisible(bool visible)
{
    sprite_.SetVisible(visible);
    shadowRenderer_.SetVisible(visible);
}

void UnitVisualComponent::SetHitboxVisible(bool visible)
{
    if (hitbox_) hitbox_->SetUnitVisible(visible);
}

void UnitVisualComponent::SetDissolveAmount(float amount)
{
    sprite_.SetDissolveAmount(amount);
    shadowRenderer_.SetDissolveAmount(amount);
}

void UnitVisualComponent::SetShadowVisible(bool visible)
{
    shadowRenderer_.SetVisible(visible);
}

bool UnitVisualComponent::PlayAnimation(const std::string& tagName, bool restart)
{
    return sprite_.Play(tagName, restart);
}

float UnitVisualComponent::GetAnimationDurationSeconds(const std::string& tagName) const
{
    return sprite_.GetAnimationDurationSeconds(tagName);
}

void UnitVisualComponent::SetFacingFromHorizontalMovement(float movementX)
{
    if (movementX == 0.0f) return;
    sprite_.SetFlipX(movementX < 0.0f);
}

void UnitVisualComponent::SetFlash(const DirectX::XMFLOAT4& color, float amount)
{
    sprite_.SetFlash(color, amount);
}

void UnitVisualComponent::SyncShadow()
{
    const auto& unitPosition = GetOwner().GetTransform().GetPosition();
    auto shadowPosition = shadow_.GetTransform().GetPosition();
    shadowPosition.x = unitPosition.x;
    shadowPosition.z = unitPosition.z;
    shadow_.GetTransform().SetPosition(shadowPosition);
}
