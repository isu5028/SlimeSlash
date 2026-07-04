#pragma once

#include "Component.h"

class UnitHitboxComponent : public Component
{
public:
    explicit UnitHitboxComponent(GameObject& owner);
    ~UnitHitboxComponent() override;

    bool Setup(
        Renderer& renderer,
        float radius,
        float height,
        const DirectX::XMFLOAT4& debugColor,
        bool debugVisible);
    void Draw(Renderer& renderer) override;

    float GetRadius() const { return radius_; }
    float GetHeight() const { return height_; }
    void SetUnitVisible(bool visible) { unitVisible_ = visible; }

private:
    static constexpr int SegmentCount = 24;
    static constexpr int VertexCount = SegmentCount * 2 + 1;
    static constexpr int IndexCount = SegmentCount * 9;

    void SetupWorldTransform(Renderer& renderer);

    ID3D11Buffer* vertexBuffer_ = nullptr;
    ID3D11Buffer* indexBuffer_ = nullptr;
    float radius_ = 0.0f;
    float height_ = 0.0f;
    bool debugVisible_ = false;
    bool unitVisible_ = true;
};
