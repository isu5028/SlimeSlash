#pragma once

#include "Component.h"

class ColoredDiskComponent : public Component
{
public:
    explicit ColoredDiskComponent(GameObject& owner);
    ~ColoredDiskComponent() override;

    bool Setup(Renderer& renderer, float radius, const DirectX::XMFLOAT4& color);
    void Draw(Renderer& renderer) override;
    void SetVisible(bool visible) { visible_ = visible; }

private:
    static constexpr int SegmentCount = 32;
    static constexpr int VertexCount = SegmentCount + 2;
    static constexpr int IndexCount = SegmentCount * 3;

    void SetupWorldTransform(Renderer& renderer);

    ID3D11Buffer* vertexBuffer_ = nullptr;
    ID3D11Buffer* indexBuffer_ = nullptr;
    bool visible_ = true;
};
