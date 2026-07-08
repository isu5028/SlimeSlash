#pragma once

#include "Component.h"
#include "Shader.h"

#include <cstdint>
#include <string>

class ImageSpriteComponent : public Component
{
public:
    explicit ImageSpriteComponent(GameObject& owner);
    ~ImageSpriteComponent() override;

    bool Setup(Renderer& renderer, const std::string& filePath, float worldHeight);
    bool SetupSolidColor(Renderer& renderer, float worldHeight);
    void SetColor(const DirectX::XMFLOAT4& color) { color_ = color; }
    void SetVisible(bool visible) { visible_ = visible; }
    float GetWorldHeight() const { return worldHeight_; }

    void Draw(Renderer& renderer) override;

private:
    bool CreateTexture(Renderer& renderer, const std::wstring& filePath);
    bool CreateSolidColorTexture(Renderer& renderer);
    bool CreateGeometry(Renderer& renderer);
    bool CreateShaders(Renderer& renderer);
    bool CreateSampler(Renderer& renderer);
    bool CreateColorBuffer(Renderer& renderer);
    void UpdateVertices(Renderer& renderer);
    void SetupWorldTransform(Renderer& renderer);
    void ReleaseResources();

    ID3D11Texture2D* texture_ = nullptr;
    ID3D11ShaderResourceView* textureView_ = nullptr;
    ID3D11Buffer* vertexBuffer_ = nullptr;
    ID3D11Buffer* indexBuffer_ = nullptr;
    ID3D11SamplerState* sampler_ = nullptr;
    ID3D11Buffer* colorBuffer_ = nullptr;
    Shader shader_;
    std::uint32_t textureWidth_ = 0;
    std::uint32_t textureHeight_ = 0;
    float worldHeight_ = 1.0f;
    bool initialized_ = false;
    bool visible_ = true;
    DirectX::XMFLOAT4 color_ = { 1.0f, 1.0f, 1.0f, 1.0f };
};
