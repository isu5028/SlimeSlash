#pragma once

#include "Component.h"

class ColoredBoxComponent : public Component
{
public:
    explicit ColoredBoxComponent(GameObject& owner);
    ~ColoredBoxComponent() override;

    bool Setup(
        Renderer& renderer,
        float width,
        float height,
        float depth,
        const DirectX::XMFLOAT4& topColor);
    void Draw(Renderer& renderer) override;

private:
    void SetupWorldTransform(Renderer& renderer);

    ID3D11Buffer* vertexBuffer_ = nullptr;
    ID3D11Buffer* indexBuffer_ = nullptr;
};
