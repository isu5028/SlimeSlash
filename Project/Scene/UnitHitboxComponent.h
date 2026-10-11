#pragma once

#include "Component.h"

class UnitHitboxComponent : public Component
{
public:
    static constexpr float DepthRadiusScale = 0.5f;

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
    float GetDepthRadius() const { return radius_ * DepthRadiusScale; }
    float GetHeight() const { return height_; }
    float GetDirectionalRadius(float offsetX, float offsetZ) const;
    void SetUnitVisible(bool visible) { unitVisible_ = visible; }

private:
    // 当たり判定の大きさを提供するComponent
    // 実際の攻撃・接触判定はPlayerCombatResolverやSceneHazardSystemがこの値を使って計算する
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
