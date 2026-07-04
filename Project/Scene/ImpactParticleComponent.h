#pragma once

#include "Component.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <vector>

class Camera;

class ImpactParticleComponent : public Component
{
public:
    ImpactParticleComponent(GameObject& owner, Camera& camera);
    ~ImpactParticleComponent() override;

    bool Setup(Renderer& renderer);
    void Emit(
        const DirectX::XMFLOAT3& position,
        const DirectX::XMFLOAT2& direction,
        bool strong,
        const DirectX::XMFLOAT4& color);
    void Update() override;
    void Draw(Renderer& renderer) override;

private:
    struct Particle
    {
        DirectX::XMFLOAT3 position = {};
        DirectX::XMFLOAT3 velocity = {};
        DirectX::XMFLOAT4 color = {};
        float age = 0.0f;
        float lifetime = 0.0f;
        float size = 0.0f;
        float rotation = 0.0f;
        float angularVelocity = 0.0f;
    };

    static constexpr int CircleSegments = 4;
    static constexpr int MaxParticles = 128;
    static constexpr int VerticesPerParticle = CircleSegments + 1;
    static constexpr int IndicesPerParticle = CircleSegments * 3;
    static constexpr int MaxVertexCount = MaxParticles * VerticesPerParticle;
    static constexpr int MaxIndexCount = MaxParticles * IndicesPerParticle;

    void SetupWorldTransform(Renderer& renderer);

    Camera& camera_;
    ID3D11Buffer* vertexBuffer_ = nullptr;
    ID3D11Buffer* indexBuffer_ = nullptr;
    std::vector<Particle> particles_;
    std::uint32_t emissionCounter_ = 0;
    std::chrono::steady_clock::time_point previousUpdate_;
};
