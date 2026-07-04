#pragma once

#include "Component.h"

#include <array>
#include <chrono>
#include <vector>

class GameObject;

class SlashAttackEffectComponent : public Component
{
public:
    SlashAttackEffectComponent(GameObject& owner, GameObject& target);
    ~SlashAttackEffectComponent() override;

    bool Setup(Renderer& renderer);
    void SetHitboxDebugVisible(bool visible) { hitboxDebugVisible_ = visible; }
    void Trigger(const DirectX::XMFLOAT2& direction, int comboStage = 1);
    void Update() override;
    void Draw(Renderer& renderer) override;

private:
    struct EffectInstance
    {
        DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 attackOrigin = { 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT2 direction = { 0.0f, 1.0f };
        DirectX::XMFLOAT4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
        float scale = 1.0f;
        float elapsed = 0.0f;
        int comboStage = 1;
        bool reverseArcAnimation = false;
    };

    static constexpr int SegmentCount = 20;
    static constexpr int VertexCount = (SegmentCount + 1) * 2;
    static constexpr int IndexCount = SegmentCount * 6;
    static constexpr int DebugVertexCount = SegmentCount + 2;
    static constexpr int DebugIndexCount = SegmentCount * 3;

    void SetupWorldTransform(Renderer& renderer, const DirectX::XMFLOAT3& position);
    void UpdateVertices(const EffectInstance& effect);
    void UpdateDebugVertices(const EffectInstance& effect);
    float EaseOutCubic(float t) const;
    float EaseInCubic(float t) const;
    float EaseInOutQuad(float t) const;

    GameObject& target_;
    ID3D11Buffer* vertexBuffer_ = nullptr;
    ID3D11Buffer* indexBuffer_ = nullptr;
    ID3D11Buffer* debugIndexBuffer_ = nullptr;
    std::array<Vertex, VertexCount> vertices_ = {};
    std::array<Vertex, DebugVertexCount> debugVertices_ = {};
    std::vector<EffectInstance> effects_;
    bool hitboxDebugVisible_ = false;
    std::chrono::steady_clock::time_point previousUpdate_;
};
