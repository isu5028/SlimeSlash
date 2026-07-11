#pragma once

#include "Component.h"
#include "Shader.h"

#include <cstdint>
#include <string>

class WorldTextSpriteComponent : public Component
{
public:
    enum class HorizontalAlignment
    {
        Center,
        Right,
    };

    explicit WorldTextSpriteComponent(GameObject& owner);
    ~WorldTextSpriteComponent() override;

    bool Setup(
        Renderer& renderer,
        const std::string& fontPath,
        const std::wstring& text,
        float worldHeight,
        const DirectX::XMFLOAT4& color);
    void SetHorizontalAlignment(HorizontalAlignment alignment)
    {
        horizontalAlignment_ = alignment;
    }
    void SetColor(const DirectX::XMFLOAT4& color) { color_ = color; }
    void SetVisible(bool visible) { visible_ = visible; }
    float GetTexturePixelWorldSize() const
    {
        return textureHeight_ == 0 ? 0.0f :
            worldHeight_ / static_cast<float>(textureHeight_);
    }
    float GetWorldWidth() const
    {
        return textureHeight_ == 0 ? 0.0f :
            worldHeight_ * (static_cast<float>(textureWidth_) /
                static_cast<float>(textureHeight_));
    }
    float GetContentWorldWidth() const
    {
        return textureHeight_ == 0 ? 0.0f :
            worldHeight_ * (textContentWidthPixels_ /
                static_cast<float>(textureHeight_));
    }
    float GetContentCenterOffsetWorldX() const
    {
        if (textureHeight_ == 0) return 0.0f;
        const float contentCenterPixels =
            textContentLeftPixels_ + textContentWidthPixels_ * 0.5f;
        const float textureCenterPixels = static_cast<float>(textureWidth_) * 0.5f;
        return worldHeight_ * ((contentCenterPixels - textureCenterPixels) /
            static_cast<float>(textureHeight_));
    }

    void Draw(Renderer& renderer) override;

private:
    bool CreateTextTexture(
        Renderer& renderer,
        const std::wstring& fontFilePath,
        const std::wstring& text,
        const DirectX::XMFLOAT4& color);
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
    std::string lastError_;
    std::uint32_t textureWidth_ = 0;
    std::uint32_t textureHeight_ = 0;
    float textContentLeftPixels_ = 0.0f;
    float textContentWidthPixels_ = 0.0f;
    float worldHeight_ = 1.0f;
    bool initialized_ = false;
    bool visible_ = true;
    HorizontalAlignment horizontalAlignment_ = HorizontalAlignment::Center;
    DirectX::XMFLOAT4 color_ = { 1.0f, 1.0f, 1.0f, 1.0f };
};
