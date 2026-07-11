#pragma once

class ColoredDiskComponent;
class ColoredPlaneComponent;
class GameObject;

class Slime5TelegraphController
{
public:
    Slime5TelegraphController(
        GameObject& linearArea,
        ColoredPlaneComponent& linearRenderer,
        GameObject& radialArea,
        ColoredDiskComponent& radialRenderer);

    void HideLinear();
    void HideRadial();
    void UpdateLinear(
        const DirectX::XMFLOAT3& position,
        const DirectX::XMFLOAT2& direction,
        float lengthScale);
    void UpdateRadial(const DirectX::XMFLOAT3& position, float radiusScale);

private:
    GameObject& linearArea_;
    ColoredPlaneComponent& linearRenderer_;
    GameObject& radialArea_;
    ColoredDiskComponent& radialRenderer_;
};
