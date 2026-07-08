#include "UnitHitboxComponent.h"

#include "GameObject.h"
#include "Renderer.h"
#include "Vertex.h"

#include <array>
#include <cmath>
#include <cstdint>

UnitHitboxComponent::UnitHitboxComponent(GameObject& owner)
    : Component(owner)
{
}

UnitHitboxComponent::~UnitHitboxComponent()
{
    DX_SAFE_RELEASE(indexBuffer_);
    DX_SAFE_RELEASE(vertexBuffer_);
}

bool UnitHitboxComponent::Setup(
    Renderer& renderer,
    float radius,
    float height,
    const DirectX::XMFLOAT4& debugColor,
    bool debugVisible)
{
    if (radius <= 0.0f || height <= 0.0f) return false;
    radius_ = radius;
    height_ = height;
    debugVisible_ = debugVisible;

    std::array<Vertex, VertexCount> vertices = {};
    for (int segment = 0; segment < SegmentCount; ++segment)
    {
        const float angle = DirectX::XM_2PI * static_cast<float>(segment) / SegmentCount;
        const float x = std::sin(angle) * radius_;
        const float z = std::cos(angle) * GetDepthRadius();
        vertices[segment * 2 + 0].Position = { x, 0.0f, z };
        vertices[segment * 2 + 1].Position = { x, height_, z };
        vertices[segment * 2 + 0].Color = debugColor;
        vertices[segment * 2 + 1].Color = debugColor;
    }
    vertices[VertexCount - 1].Position = { 0.0f, height_, 0.0f };
    vertices[VertexCount - 1].Color = debugColor;

    std::array<std::uint16_t, IndexCount> indices = {};
    for (int segment = 0; segment < SegmentCount; ++segment)
    {
        const std::uint16_t bottom = static_cast<std::uint16_t>(segment * 2);
        const std::uint16_t top = static_cast<std::uint16_t>(bottom + 1);
        const std::uint16_t nextBottom = static_cast<std::uint16_t>(((segment + 1) % SegmentCount) * 2);
        const std::uint16_t nextTop = static_cast<std::uint16_t>(nextBottom + 1);
        const int index = segment * 9;
        indices[index + 0] = bottom;
        indices[index + 1] = nextBottom;
        indices[index + 2] = top;
        indices[index + 3] = top;
        indices[index + 4] = nextBottom;
        indices[index + 5] = nextTop;
        indices[index + 6] = static_cast<std::uint16_t>(VertexCount - 1);
        indices[index + 7] = top;
        indices[index + 8] = nextTop;
    }

    D3D11_BUFFER_DESC vertexDescription = {};
    vertexDescription.ByteWidth = sizeof(vertices);
    vertexDescription.Usage = D3D11_USAGE_IMMUTABLE;
    vertexDescription.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vertexData = {};
    vertexData.pSysMem = vertices.data();
    if (FAILED(renderer.GetDevice()->CreateBuffer(
        &vertexDescription, &vertexData, &vertexBuffer_))) return false;

    D3D11_BUFFER_DESC indexDescription = {};
    indexDescription.ByteWidth = sizeof(indices);
    indexDescription.Usage = D3D11_USAGE_IMMUTABLE;
    indexDescription.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA indexData = {};
    indexData.pSysMem = indices.data();
    return SUCCEEDED(renderer.GetDevice()->CreateBuffer(
        &indexDescription, &indexData, &indexBuffer_));
}

float UnitHitboxComponent::GetDirectionalRadius(float offsetX, float offsetZ) const
{
    const float length = std::sqrt(offsetX * offsetX + offsetZ * offsetZ);
    if (length <= 0.0001f) return radius_;

    const float directionX = offsetX / length;
    const float directionZ = offsetZ / length;
    const float depthRadius = GetDepthRadius();
    return std::sqrt(
        radius_ * radius_ * directionX * directionX +
        depthRadius * depthRadius * directionZ * directionZ);
}

void UnitHitboxComponent::SetupWorldTransform(Renderer& renderer)
{
    auto& constantBuffer = renderer.GetRenderParam().CbTransformSet;
    DirectX::XMStoreFloat4x4(&constantBuffer.Data.Transform,
        DirectX::XMMatrixTranspose(GetOwner().GetTransform().GetWorldMatrix()));

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    auto* context = renderer.GetDeviceContext();
    if (SUCCEEDED(context->Map(constantBuffer.pBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        CopyMemory(mapped.pData, &constantBuffer.Data, sizeof(constantBuffer.Data));
        context->Unmap(constantBuffer.pBuffer, 0);
        context->VSSetConstantBuffers(0, 1, &constantBuffer.pBuffer);
    }
}

void UnitHitboxComponent::Draw(Renderer& renderer)
{
    if (!unitVisible_ || !debugVisible_ || !vertexBuffer_ || !indexBuffer_) return;
    renderer.ApplyDefaultPipeline();
    SetupWorldTransform(renderer);

    auto* context = renderer.GetDeviceContext();
    const UINT stride = sizeof(Vertex);
    const UINT offset = 0;
    context->IASetVertexBuffers(0, 1, &vertexBuffer_, &stride, &offset);
    context->IASetIndexBuffer(indexBuffer_, DXGI_FORMAT_R16_UINT, 0);
    context->DrawIndexed(IndexCount, 0, 0);
}
