#pragma once

#include "Component.h"
#include "Shader.h"

// XZ平面に単色の長方形を描くコンポーネント。
// 地面と簡易的な足元の影で共通利用する。
class ColoredPlaneComponent : public Component
{
public:
    explicit ColoredPlaneComponent(GameObject& owner);
    ~ColoredPlaneComponent() override;

    bool Setup(Renderer& renderer, float width, float depth, const DirectX::XMFLOAT4& color);
    void Draw(Renderer& renderer) override;
    void SetVisible(bool visible) { visible_ = visible; }
    void SetDissolveAmount(float amount);
    void SetDissolvePixelSize(float width, float depth);
    void SetUniformDissolve(bool uniform) { uniformDissolve_ = uniform; }
    void SetOpacity(float opacity);

private:
    void SetupWorldTransform(Renderer& renderer);
    bool CreateDitherResources(Renderer& renderer);

    ID3D11Buffer* vertexBuffer_ = nullptr;
    ID3D11Buffer* indexBuffer_ = nullptr;
    ID3D11Buffer* ditherBuffer_ = nullptr;
    Shader ditherShader_;
    float halfWidth_ = 0.5f;
    float halfDepth_ = 0.5f;
    float dissolveAmount_ = 0.0f;
    float dissolvePixelWidth_ = 0.0625f;
    float dissolvePixelDepth_ = 0.0625f;
    float opacity_ = 1.0f;
    bool uniformDissolve_ = false;
    bool visible_ = true;
};
