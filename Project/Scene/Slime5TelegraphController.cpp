#include "Slime5TelegraphController.h"

#include "ColoredDiskComponent.h"
#include "ColoredPlaneComponent.h"
#include "GameObject.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr float DashDistance = 4.9f;
    constexpr float TelegraphWidth = 0.61333334f;
    constexpr float TelegraphHeight = -0.99f;
    constexpr float TelegraphAnchorOffsetZ = 0.6f;
    constexpr float RadialTelegraphRadius = 3.3f;
}

Slime5TelegraphController::Slime5TelegraphController(
    GameObject& linearArea,
    ColoredPlaneComponent& linearRenderer,
    GameObject& radialArea,
    ColoredDiskComponent& radialRenderer)
    : linearArea_(linearArea)
    , linearRenderer_(linearRenderer)
    , radialArea_(radialArea)
    , radialRenderer_(radialRenderer)
{
}

void Slime5TelegraphController::HideLinear()
{
    linearRenderer_.SetVisible(false);
}

void Slime5TelegraphController::HideRadial()
{
    radialRenderer_.SetVisible(false);
}

void Slime5TelegraphController::UpdateLinear(
    const DirectX::XMFLOAT3& position,
    const DirectX::XMFLOAT2& direction,
    float lengthScale)
{
    const float scale = std::clamp(lengthScale, 0.0f, 1.0f);
    if (scale <= 0.0f)
    {
        HideLinear();
        return;
    }
    const float length = DashDistance * scale;
    linearArea_.GetTransform().SetPosition({
        position.x + direction.x * length * 0.5f,
        TelegraphHeight,
        position.z + TelegraphAnchorOffsetZ + direction.y * length * 0.5f
    });
    linearArea_.GetTransform().SetRotation({ 0.0f, std::atan2(direction.x, direction.y), 0.0f });
    linearArea_.GetTransform().SetScale({ TelegraphWidth, 1.0f, length });
    linearRenderer_.SetVisible(true);
}

void Slime5TelegraphController::UpdateRadial(
    const DirectX::XMFLOAT3& position, float radiusScale)
{
    const float scale = std::clamp(radiusScale, 0.0f, 1.0f);
    if (scale <= 0.0f)
    {
        HideRadial();
        return;
    }
    radialArea_.GetTransform().SetPosition({ position.x, TelegraphHeight, position.z });
    radialArea_.GetTransform().SetScale({
        RadialTelegraphRadius * scale, 1.0f, RadialTelegraphRadius * scale
    });
    radialRenderer_.SetVisible(true);
}
