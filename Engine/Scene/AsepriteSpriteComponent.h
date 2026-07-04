#pragma once

#include "AsepriteLoader.h"
#include "Component.h"
#include "Shader.h"

#include <chrono>
#include <cstddef>
#include <string>

class AsepriteSpriteComponent : public Component
{
public:
    explicit AsepriteSpriteComponent(GameObject& owner);
    ~AsepriteSpriteComponent() override;

    bool Setup(Renderer& renderer, const std::string& filePath, float worldHeight = 1.0f);
    bool Play(const std::string& tagName, bool restart = false);
    void Update() override;
    void Draw(Renderer& renderer) override;

    const std::string& GetLastError() const { return lastError_; }
    std::size_t GetCurrentFrame() const { return currentFrame_; }

private:
    bool CreateTexture(Renderer& renderer);
    bool CreateGeometry(Renderer& renderer);
    bool CreateShaders(Renderer& renderer);
    bool CreateSampler(Renderer& renderer);
    void UpdateVertices(Renderer& renderer);
    void SetupWorldTransform(Renderer& renderer);
    void ReleaseResources();
    void AdvanceFrame();

    AsepriteData data_;
    Shader shader_;
    ID3D11Buffer* vertexBuffer_ = nullptr;
    ID3D11Buffer* indexBuffer_ = nullptr;
    ID3D11Texture2D* texture_ = nullptr;
    ID3D11ShaderResourceView* textureView_ = nullptr;
    ID3D11SamplerState* sampler_ = nullptr;
    const AsepriteTag* activeTag_ = nullptr;
    std::size_t currentFrame_ = 0;
    int playbackStep_ = 1;
    float worldHeight_ = 1.0f;
    bool verticesDirty_ = true;
    bool initialized_ = false;
    std::string lastError_;
    std::chrono::steady_clock::time_point previousUpdate_;
    std::chrono::milliseconds frameElapsed_{ 0 };
};
