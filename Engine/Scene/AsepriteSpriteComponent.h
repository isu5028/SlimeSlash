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

    // Asepriteを読み、描画に必要なGPUリソースをすべて生成する
    // worldHeightは3D空間に置く板ポリゴンの高さ
    bool Setup(Renderer& renderer, const std::string& filePath, float worldHeight = 1.0f);

    // Asepriteで設定したタグ名のアニメーションへ切り替える
    // restart=falseで既に同じタグを再生中なら、現在位置を維持する
    bool Play(const std::string& tagName, bool restart = false);
    float GetAnimationDurationSeconds(const std::string& tagName) const;
    bool SetFrame(std::size_t frameIndex);
    bool GetOpaqueHorizontalBounds(
        std::size_t frameIndex, float& start, float& end) const;
    void SetHorizontalRange(float start, float end);
    void SetFlipX(bool flip);
    void SetVisible(bool visible) { visible_ = visible; }
    void SetFlash(const DirectX::XMFLOAT4& color, float amount);
    void SetDissolveAmount(float amount);
    void Update() override;
    void Draw(Renderer& renderer) override;

    const std::string& GetLastError() const { return lastError_; }
    std::size_t GetCurrentFrame() const { return currentFrame_; }

private:
    // CPU上でスプライトシートを作り、Texture2DとSRVへ変換する
    bool CreateTexture(Renderer& renderer);
    // 板ポリゴン用の頂点・インデックスバッファを作る
    bool CreateGeometry(Renderer& renderer);
    bool CreateShaders(Renderer& renderer);
    bool CreateSampler(Renderer& renderer);
    bool CreateColorBuffer(Renderer& renderer);
    void UpdateVertices(Renderer& renderer);
    void SetupWorldTransform(Renderer& renderer);
    void ReleaseResources();
    void AdvanceFrame();

    // CPU側のAsepriteデータと、GPU側の描画リソース
    AsepriteData data_;
    Shader shader_;
    ID3D11Buffer* vertexBuffer_ = nullptr;
    ID3D11Buffer* indexBuffer_ = nullptr;
    ID3D11Texture2D* texture_ = nullptr;
    ID3D11ShaderResourceView* textureView_ = nullptr;
    ID3D11SamplerState* sampler_ = nullptr;
    ID3D11Buffer* colorBuffer_ = nullptr;
    // 現在再生しているタグ、フレーム番号、進行方向
    const AsepriteTag* activeTag_ = nullptr;
    std::size_t currentFrame_ = 0;
    int playbackStep_ = 1;
    float worldHeight_ = 1.0f;
    bool flipX_ = false;
    bool manualFrame_ = false;
    bool playbackCompleted_ = false;
    float horizontalStart_ = 0.0f;
    float horizontalEnd_ = 1.0f;
    DirectX::XMFLOAT4 flashColor_ = { 1.0f, 1.0f, 1.0f, 1.0f };
    float flashAmount_ = 0.0f;
    float dissolveAmount_ = 0.0f;
    // フレーム変更時だけUVを書き換えるための更新フラグ
    bool verticesDirty_ = true;
    bool initialized_ = false;
    bool visible_ = true;
    std::string lastError_;
    // フレームレートに依存せず、Aseprite指定時間で再生するための時計
    std::chrono::steady_clock::time_point previousUpdate_;
    std::chrono::milliseconds frameElapsed_{ 0 };
};
