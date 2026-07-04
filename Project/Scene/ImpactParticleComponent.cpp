#include "ImpactParticleComponent.h"

#include "Camera.h"
#include "GameTime.h"
#include "Renderer.h"
#include "Vertex.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

ImpactParticleComponent::ImpactParticleComponent(GameObject& owner, Camera& camera)
    : Component(owner)
    , camera_(camera)
    , previousUpdate_(std::chrono::steady_clock::now())
{
}

ImpactParticleComponent::~ImpactParticleComponent()
{
    DX_SAFE_RELEASE(indexBuffer_);
    DX_SAFE_RELEASE(vertexBuffer_);
}

bool ImpactParticleComponent::Setup(Renderer& renderer)
{
    D3D11_BUFFER_DESC vertexDescription = {};
    vertexDescription.ByteWidth = sizeof(Vertex) * MaxVertexCount;
    vertexDescription.Usage = D3D11_USAGE_DYNAMIC;
    vertexDescription.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vertexDescription.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(renderer.GetDevice()->CreateBuffer(
        &vertexDescription, nullptr, &vertexBuffer_))) return false;

    std::array<std::uint16_t, MaxIndexCount> indices = {};
    for (int particle = 0; particle < MaxParticles; ++particle)
    {
        const int vertexBase = particle * VerticesPerParticle;
        const int indexBase = particle * IndicesPerParticle;
        for (int segment = 0; segment < CircleSegments; ++segment)
        {
            const int index = indexBase + segment * 3;
            indices[index + 0] = static_cast<std::uint16_t>(vertexBase);
            indices[index + 1] = static_cast<std::uint16_t>(
                vertexBase + 1 + (segment + 1) % CircleSegments);
            indices[index + 2] = static_cast<std::uint16_t>(vertexBase + 1 + segment);
        }
    }

    D3D11_BUFFER_DESC indexDescription = {};
    indexDescription.ByteWidth = sizeof(indices);
    indexDescription.Usage = D3D11_USAGE_IMMUTABLE;
    indexDescription.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA indexData = {};
    indexData.pSysMem = indices.data();
    return SUCCEEDED(renderer.GetDevice()->CreateBuffer(
        &indexDescription, &indexData, &indexBuffer_));
}

void ImpactParticleComponent::Emit(
    const DirectX::XMFLOAT3& position,
    const DirectX::XMFLOAT2& direction,
    bool strong,
    const DirectX::XMFLOAT4& color)
{
    const float length = std::sqrt(direction.x * direction.x + direction.y * direction.y);
    if (length <= 0.0001f) return;
    const DirectX::XMFLOAT2 forward = { direction.x / length, direction.y / length };
    const DirectX::XMFLOAT2 side = { -forward.y, forward.x };
    const int count = strong ? 4 : 2;
    const float speed = strong ? 8.2f : 5.2f;

    for (int index = 0; index < count && particles_.size() < MaxParticles; ++index)
    {
        const float spread = count > 1
            ? static_cast<float>(index) / static_cast<float>(count - 1) * 2.0f - 1.0f
            : 0.0f;
        const float alternating = (index % 2 == 0) ? 1.0f : -1.0f;
        const float speedVariation = 0.72f + 0.28f * static_cast<float>((index * 7) % 11) / 10.0f;
        Particle particle;
        particle.position = position;
        particle.position.x += side.x * spread * 0.18f;
        particle.position.z += side.y * spread * 0.18f;
        particle.velocity = {
            (forward.x * (0.72f + 0.18f * alternating) + side.x * spread * 0.85f) * speed * speedVariation,
            (0.45f + (1.0f - std::abs(spread)) * 0.75f) * speed,
            (forward.y * (0.72f + 0.18f * alternating) + side.y * spread * 0.85f) * speed * speedVariation
        };
        particle.color = color;
        particle.lifetime = strong ? 0.78f : 0.62f;
        particle.size = (strong ? 0.1785f : 0.1365f) *
            (0.80f + 0.35f * static_cast<float>((index * 5) % 9) / 8.0f);
        const std::uint32_t rotationSeed =
            (++emissionCounter_ * 1664525u + 1013904223u);
        const float rotationRandom = static_cast<float>(rotationSeed & 0xffffu) / 65535.0f;
        const float speedRandom = static_cast<float>((rotationSeed >> 16) & 0xffffu) / 65535.0f;
        particle.rotation = rotationRandom * DirectX::XM_2PI;
        particle.angularVelocity = (speedRandom < 0.5f ? -1.0f : 1.0f) *
            (5.0f + std::abs(speedRandom - 0.5f) * 8.0f);
        particles_.push_back(particle);
    }
}

void ImpactParticleComponent::Update()
{
    const auto now = std::chrono::steady_clock::now();
    float deltaTime = std::chrono::duration<float>(now - previousUpdate_).count();
    previousUpdate_ = now;
    if (GameTime::IsHitStopped()) return;
    deltaTime = std::min(deltaTime, 0.05f);

    for (auto& particle : particles_)
    {
        particle.age += deltaTime;
        particle.rotation += particle.angularVelocity * deltaTime;
        particle.velocity.y -= 8.0f * deltaTime;
        const float drag = std::max(0.0f, 1.0f - 3.4f * deltaTime);
        particle.velocity.x *= drag;
        particle.velocity.z *= drag;
        particle.position.x += particle.velocity.x * deltaTime;
        particle.position.y += particle.velocity.y * deltaTime;
        particle.position.z += particle.velocity.z * deltaTime;
    }
    particles_.erase(std::remove_if(particles_.begin(), particles_.end(),
        [](const Particle& particle) { return particle.age >= particle.lifetime; }),
        particles_.end());
}

void ImpactParticleComponent::SetupWorldTransform(Renderer& renderer)
{
    auto& constantBuffer = renderer.GetRenderParam().CbTransformSet;
    DirectX::XMStoreFloat4x4(&constantBuffer.Data.Transform,
        DirectX::XMMatrixTranspose(DirectX::XMMatrixIdentity()));
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    auto* context = renderer.GetDeviceContext();
    if (SUCCEEDED(context->Map(constantBuffer.pBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        CopyMemory(mapped.pData, &constantBuffer.Data, sizeof(constantBuffer.Data));
        context->Unmap(constantBuffer.pBuffer, 0);
        context->VSSetConstantBuffers(0, 1, &constantBuffer.pBuffer);
    }
}

void ImpactParticleComponent::Draw(Renderer& renderer)
{
    if (particles_.empty() || !vertexBuffer_ || !indexBuffer_) return;

    const auto inverseView = DirectX::XMMatrixInverse(nullptr, camera_.GetViewMatrix());
    DirectX::XMFLOAT3 right = {};
    DirectX::XMFLOAT3 up = {};
    DirectX::XMStoreFloat3(&right, DirectX::XMVector3Normalize(
        DirectX::XMVector3TransformNormal(DirectX::XMVectorSet(1, 0, 0, 0), inverseView)));
    DirectX::XMStoreFloat3(&up, DirectX::XMVector3Normalize(
        DirectX::XMVector3TransformNormal(DirectX::XMVectorSet(0, 1, 0, 0), inverseView)));

    std::array<Vertex, MaxVertexCount> vertices = {};
    for (std::size_t particleIndex = 0; particleIndex < particles_.size(); ++particleIndex)
    {
        const auto& particle = particles_[particleIndex];
        const int base = static_cast<int>(particleIndex) * VerticesPerParticle;
        const float normalizedAge = particle.age / particle.lifetime;
        const float shrinkProgress = std::max(0.0f, std::min(
            (normalizedAge - 0.68f) / 0.32f, 1.0f));
        const float smoothShrink =
            shrinkProgress * shrinkProgress * (3.0f - 2.0f * shrinkProgress);
        const float sizeScale = 1.0f - smoothShrink;
        const float fadeProgress = std::max(0.0f, std::min(
            (normalizedAge - 0.55f) / 0.45f, 1.0f));
        const float alphaScale = 1.0f -
            fadeProgress * fadeProgress * (3.0f - 2.0f * fadeProgress);
        auto color = particle.color;
        color.w *= alphaScale;
        vertices[base].Position = particle.position;
        vertices[base].Color = color;
        for (int segment = 0; segment < CircleSegments; ++segment)
        {
            const float angle = particle.rotation + DirectX::XM_PIDIV4 +
                DirectX::XM_2PI * static_cast<float>(segment) / CircleSegments;
            const float x = std::cos(angle) * particle.size * sizeScale;
            const float y = std::sin(angle) * particle.size * sizeScale;
            vertices[base + segment + 1].Position = {
                particle.position.x + right.x * x + up.x * y,
                particle.position.y + right.y * x + up.y * y,
                particle.position.z + right.z * x + up.z * y
            };
            vertices[base + segment + 1].Color = color;
        }
    }

    renderer.ApplyDefaultPipeline();
    SetupWorldTransform(renderer);
    auto* context = renderer.GetDeviceContext();
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (FAILED(context->Map(vertexBuffer_, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) return;
    CopyMemory(mapped.pData, vertices.data(), sizeof(Vertex) *
        particles_.size() * VerticesPerParticle);
    context->Unmap(vertexBuffer_, 0);

    const UINT stride = sizeof(Vertex);
    const UINT offset = 0;
    context->IASetVertexBuffers(0, 1, &vertexBuffer_, &stride, &offset);
    context->IASetIndexBuffer(indexBuffer_, DXGI_FORMAT_R16_UINT, 0);
    context->DrawIndexed(static_cast<UINT>(particles_.size()) * IndicesPerParticle, 0, 0);
}
